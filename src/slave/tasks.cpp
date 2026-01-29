#include "tasks.h"
#include "slave/spi_slave.h"
#include "display.h"
#include "shared/config.h"
#include "shared/protocol.h"
#include <Arduino.h>

// =============================================================================
// Global Synchronization Objects
// =============================================================================

QueueHandle_t queueSpiToDisplay = nullptr;
QueueHandle_t queueDisplayToSpi = nullptr;
SemaphoreHandle_t mutexTft = nullptr;
SemaphoreHandle_t mutexI2C = nullptr;

// =============================================================================
// Task Handles
// =============================================================================

static TaskHandle_t taskHandleSpiComm = nullptr;
static TaskHandle_t taskHandleDisplay = nullptr;
static TaskHandle_t taskHandleSerial = nullptr;

// =============================================================================
// Task Functions (Forward Declarations)
// =============================================================================

static void taskSpiComm(void* parameter);
static void taskDisplay(void* parameter);
static void taskSerial(void* parameter);

// =============================================================================
// Initialization
// =============================================================================

bool tasksInit() {
    // Create queues
    queueSpiToDisplay = xQueueCreate(QUEUE_SIZE_RPM_DATA, sizeof(SpiToDisplayMsg));
    if (queueSpiToDisplay == nullptr) {
        Serial.println("Failed to create SPI->Display queue");
        return false;
    }

    queueDisplayToSpi = xQueueCreate(QUEUE_SIZE_UI_CMD, sizeof(DisplayToSpiMsg));
    if (queueDisplayToSpi == nullptr) {
        Serial.println("Failed to create Display->SPI queue");
        return false;
    }

    // Create mutexes
    mutexTft = xSemaphoreCreateMutex();
    if (mutexTft == nullptr) {
        Serial.println("Failed to create TFT mutex");
        return false;
    }

    mutexI2C = xSemaphoreCreateMutex();
    if (mutexI2C == nullptr) {
        Serial.println("Failed to create I2C mutex");
        return false;
    }

    Serial.println("FreeRTOS objects initialized (queues, mutexes)");
    return true;
}

bool tasksStart() {
    BaseType_t result;

    // Create SPI communication task (highest priority)
    result = xTaskCreatePinnedToCore(
        taskSpiComm,
        "SPI_Comm",
        TASK_STACK_SPI_COMM,
        nullptr,
        TASK_PRIORITY_SPI_COMM,
        &taskHandleSpiComm,
        TASK_CORE_SPI_COMM
    );
    if (result != pdPASS) {
        Serial.println("Failed to create SPI task");
        return false;
    }

    // Create display task
    result = xTaskCreatePinnedToCore(
        taskDisplay,
        "Display",
        TASK_STACK_DISPLAY,
        nullptr,
        TASK_PRIORITY_DISPLAY,
        &taskHandleDisplay,
        TASK_CORE_DISPLAY
    );
    if (result != pdPASS) {
        Serial.println("Failed to create Display task");
        return false;
    }

    // Create serial task (lowest priority)
    result = xTaskCreatePinnedToCore(
        taskSerial,
        "Serial",
        TASK_STACK_SERIAL,
        nullptr,
        TASK_PRIORITY_SERIAL,
        &taskHandleSerial,
        TASK_CORE_SERIAL
    );
    if (result != pdPASS) {
        Serial.println("Failed to create Serial task");
        return false;
    }

    Serial.printf("Tasks started on cores (SPI:%d, Display:%d, Serial:%d)\n",
                  TASK_CORE_SPI_COMM, TASK_CORE_DISPLAY, TASK_CORE_SERIAL);

    return true;
}

TaskHandle_t getTaskSpiComm() { return taskHandleSpiComm; }
TaskHandle_t getTaskDisplay() { return taskHandleDisplay; }
TaskHandle_t getTaskSerial() { return taskHandleSerial; }

// =============================================================================
// SPI Communication Task
// =============================================================================
// High priority task that handles master-slave SPI communication
// Runs at 10ms intervals to ensure responsive communication

static void taskSpiComm(void* parameter) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t taskPeriod = pdMS_TO_TICKS(10);  // 100Hz polling

    SpiToDisplayMsg displayMsg;
    DisplayToSpiMsg uiMsg;
    bool wasConnected = false;

    Serial.println("[SPI Task] Started");

    while (true) {
        // Check for UI commands from display task (non-blocking)
        if (xQueueReceive(queueDisplayToSpi, &uiMsg, 0) == pdTRUE) {
            spiSlaveSetRequest(uiMsg.requestedMode, uiMsg.requestedRpm);
        }

        // Process SPI transactions
        spiSlaveProcess();

        // Check connection and send updates to display
        bool connected = spiSlaveIsConnected();
        bool reconnected = spiSlaveCheckReconnected();

        // Send update to display if data changed or on reconnection
        displayMsg.rpm = spiSlaveGetLastRpm();
        displayMsg.mode = spiSlaveGetMasterMode();
        displayMsg.connected = connected;
        displayMsg.forceRefresh = reconnected;

        // Always send on state changes, otherwise throttle
        static uint16_t lastSentRpm = 0;
        static uint8_t lastSentMode = 0;
        static bool lastSentConnected = false;

        bool stateChanged = (displayMsg.rpm != lastSentRpm) ||
                           (displayMsg.mode != lastSentMode) ||
                           (displayMsg.connected != lastSentConnected) ||
                           reconnected;

        if (stateChanged) {
            // Non-blocking send - drop if queue full (display will catch up)
            if (xQueueSend(queueSpiToDisplay, &displayMsg, 0) == pdTRUE) {
                lastSentRpm = displayMsg.rpm;
                lastSentMode = displayMsg.mode;
                lastSentConnected = displayMsg.connected;
            }
        }

        wasConnected = connected;

        // Delay until next period
        vTaskDelayUntil(&lastWakeTime, taskPeriod);
    }
}

// =============================================================================
// Display Task
// =============================================================================
// Medium priority task that handles UI rendering and touch input
// Runs at ~60Hz for smooth animations

static void taskDisplay(void* parameter) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t taskPeriod = pdMS_TO_TICKS(16);  // ~60Hz

    SpiToDisplayMsg spiMsg;

    Serial.println("[Display Task] Started");

    while (true) {
        // Check for updates from SPI task (non-blocking)
        while (xQueueReceive(queueSpiToDisplay, &spiMsg, 0) == pdTRUE) {
            // Update display with new data (TFT mutex handled inside display functions)
            if (spiMsg.forceRefresh || spiMsg.rpm != 0) {
                displayUpdateRpm(spiMsg.rpm);
            }
            displaySetConnected(spiMsg.connected);
        }

        // Process display loop (touch, animations, etc.)
        // TFT mutex is taken/released inside displayLoop
        displayLoop();

        // Delay until next frame
        vTaskDelayUntil(&lastWakeTime, taskPeriod);
    }
}

// =============================================================================
// Serial Task
// =============================================================================
// Low priority task for debug commands and statistics

static void taskSerial(void* parameter) {
    Serial.println("[Serial Task] Started");

    while (true) {
        if (Serial.available()) {
            char cmd = Serial.read();
            // Drain remaining characters
            while (Serial.available()) Serial.read();

            switch (cmd) {
                case 'c':
                case 'C':
                    Serial.println("\n=== Slave Statistics ===");
                    Serial.printf("Valid packets: %lu\n", spiSlaveGetValidPacketCount());
                    Serial.printf("Invalid packets: %lu\n", spiSlaveGetInvalidPacketCount());
                    Serial.printf("Last RPM from master: %u\n", spiSlaveGetLastRpm());
                    Serial.printf("Master mode: %s\n", spiSlaveGetMasterMode() == MODE_AUTO ? "AUTO" : "MANUAL");
                    Serial.printf("Connected: %s\n", spiSlaveIsConnected() ? "YES" : "NO");
                    Serial.printf("Time since last packet: %lu ms\n", spiSlaveGetTimeSinceLastPacket());
                    Serial.printf("Requested mode: %s\n", spiSlaveGetRequestedMode() == MODE_AUTO ? "AUTO" : "MANUAL");
                    Serial.printf("Requested RPM: %u\n", spiSlaveGetRequestedRpm());
                    Serial.println();
                    // Task stack info
                    Serial.println("=== Task Stack Info ===");
                    Serial.printf("SPI Task free stack: %u words\n", uxTaskGetStackHighWaterMark(taskHandleSpiComm));
                    Serial.printf("Display Task free stack: %u words\n", uxTaskGetStackHighWaterMark(taskHandleDisplay));
                    Serial.printf("Serial Task free stack: %u words\n", uxTaskGetStackHighWaterMark(taskHandleSerial));
                    Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
                    Serial.println();
                    break;

                case 't':
                case 'T':
                    Serial.println("\n=== Task Info ===");
                    Serial.printf("SPI Task state: %d, priority: %d\n",
                                  eTaskGetState(taskHandleSpiComm),
                                  uxTaskPriorityGet(taskHandleSpiComm));
                    Serial.printf("Display Task state: %d, priority: %d\n",
                                  eTaskGetState(taskHandleDisplay),
                                  uxTaskPriorityGet(taskHandleDisplay));
                    Serial.printf("Serial Task state: %d, priority: %d\n",
                                  eTaskGetState(taskHandleSerial),
                                  uxTaskPriorityGet(taskHandleSerial));
                    Serial.println();
                    break;

                case '?':
                case 'h':
                case 'H':
                    Serial.println("\n=== SPI Display Slave (FreeRTOS) ===");
                    Serial.println("Commands:");
                    Serial.println("  c - Show statistics");
                    Serial.println("  t - Show task info");
                    Serial.println("  ? - Show this help");
                    Serial.println();
                    break;

                default:
                    break;
            }
        }

        // Check serial less frequently to save CPU
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
