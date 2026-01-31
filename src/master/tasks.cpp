#include "tasks.h"
#include "master/spi_master.h"
#include "master/sd_handler.h"
#include "master/ota_handler.h"
#include "can_handler.h"
#include "rpm_counter.h"
#include "encoder_mux.h"
#include "shared/config.h"
#include "shared/protocol.h"
#include <Arduino.h>
#include <esp_task_wdt.h>
#include <Preferences.h>

// =============================================================================
// Global Synchronization Objects
// =============================================================================

SemaphoreHandle_t stateMutex = nullptr;
QueueHandle_t queueSlaveCmd = nullptr;
QueueHandle_t queueNvsSave = nullptr;

// Shared state
MasterState masterState = {
    .currentRpm = 0,
    .displayMode = MODE_AUTO,
    .manualRpm = 3000,
    .opMode = OP_MODE_SIMULATE,
    .health = HEALTH_OK,
    .lastValidSpiTime = 0,
    .spiTimeoutCount = 0,
    .currentPwmDuty = 0,
    .simGoingUp = true,
    .lastSimChange = 0
};

// Simulation constants
#define SIM_MIN_RPM 3500
#define SIM_MAX_RPM 4500

// =============================================================================
// Task Handles
// =============================================================================

static TaskHandle_t taskHandlePump = nullptr;
static TaskHandle_t taskHandleSpiComm = nullptr;
static TaskHandle_t taskHandleUi = nullptr;
static TaskHandle_t taskHandleNvs = nullptr;

// =============================================================================
// NVS State
// =============================================================================

static Preferences prefs;
static volatile bool nvsSavePending = false;
static volatile uint32_t lastInputTime = 0;
static uint8_t savedDisplayMode = MODE_AUTO;
static uint16_t savedManualRpm = 3000;

// =============================================================================
// RTC Memory - Survives soft reset
// =============================================================================

RTC_NOINIT_ATTR static uint32_t rtcResetCount;
RTC_NOINIT_ATTR static uint32_t rtcWdtResetCount;
RTC_NOINIT_ATTR static uint32_t rtcLastUptimeMs;
RTC_NOINIT_ATTR static uint32_t rtcMagic;
#define RTC_MAGIC_VALUE 0xDEADBEEF

// =============================================================================
// Forward Declarations
// =============================================================================

static void taskPump(void* param);
static void taskSpiComm(void* param);
static void taskUi(void* param);
static void taskNvs(void* param);

// =============================================================================
// Initialization
// =============================================================================

bool tasksInit() {
    // Create state mutex
    stateMutex = xSemaphoreCreateMutex();
    if (stateMutex == nullptr) {
        Serial.println("Failed to create state mutex");
        return false;
    }

    // Create queues
    queueSlaveCmd = xQueueCreate(QUEUE_SIZE_SLAVE_CMD, sizeof(SettingsUpdateMsg));
    if (queueSlaveCmd == nullptr) {
        Serial.println("Failed to create slave command queue");
        return false;
    }

    queueNvsSave = xQueueCreate(QUEUE_SIZE_NVS_SAVE, sizeof(NvsSaveRequest));
    if (queueNvsSave == nullptr) {
        Serial.println("Failed to create NVS save queue");
        return false;
    }

    // Initialize RTC tracking
    if (rtcMagic != RTC_MAGIC_VALUE) {
        rtcMagic = RTC_MAGIC_VALUE;
        rtcResetCount = 0;
        rtcWdtResetCount = 0;
        rtcLastUptimeMs = 0;
    }
    rtcResetCount++;

    // Log reset reason
    esp_reset_reason_t reason = esp_reset_reason();
    const char* reasonStr = "UNKNOWN";
    bool wasWdt = false;

    switch (reason) {
        case ESP_RST_POWERON:   reasonStr = "POWER_ON"; break;
        case ESP_RST_EXT:       reasonStr = "EXTERNAL"; break;
        case ESP_RST_SW:        reasonStr = "SOFTWARE"; break;
        case ESP_RST_PANIC:     reasonStr = "PANIC"; wasWdt = true; break;
        case ESP_RST_INT_WDT:   reasonStr = "INT_WDT"; wasWdt = true; break;
        case ESP_RST_TASK_WDT:  reasonStr = "TASK_WDT"; wasWdt = true; break;
        case ESP_RST_WDT:       reasonStr = "WDT"; wasWdt = true; break;
        case ESP_RST_DEEPSLEEP: reasonStr = "DEEPSLEEP"; break;
        case ESP_RST_BROWNOUT:  reasonStr = "BROWNOUT"; break;
        default: break;
    }

    if (wasWdt) {
        rtcWdtResetCount++;
    }

    Serial.printf("Reset reason: %s (total: %lu, WDT: %lu)\n",
                  reasonStr, rtcResetCount, rtcWdtResetCount);
    if (rtcLastUptimeMs > 0) {
        Serial.printf("Last uptime: %lu ms\n", rtcLastUptimeMs);
    }

    // Load settings from NVS
    prefs.begin("master", true);
    masterState.displayMode = prefs.getUChar("mode", MODE_AUTO);
    masterState.manualRpm = prefs.getUShort("manualRpm", 3000);
    prefs.end();

    savedDisplayMode = masterState.displayMode;
    savedManualRpm = masterState.manualRpm;

    Serial.printf("Loaded: mode=%s, manualRpm=%u\n",
                  masterState.displayMode == MODE_AUTO ? "AUTO" : "MANUAL",
                  masterState.manualRpm);

    // Initialize timing
    masterState.lastSimChange = millis();
    masterState.lastValidSpiTime = millis();
    masterState.currentRpm = SIM_MIN_RPM;

    Serial.println("FreeRTOS objects initialized");
    return true;
}

bool tasksStart() {
    BaseType_t result;

    // Create pump control task (highest priority, safety critical)
    result = xTaskCreatePinnedToCore(
        taskPump,
        "Pump",
        TASK_STACK_PUMP,
        nullptr,
        TASK_PRIORITY_PUMP,
        &taskHandlePump,
        TASK_CORE_PUMP
    );
    if (result != pdPASS) {
        Serial.println("Failed to create Pump task");
        return false;
    }

    // Create SPI communication task
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

    // Create UI task (encoder + serial)
    result = xTaskCreatePinnedToCore(
        taskUi,
        "UI",
        TASK_STACK_UI,
        nullptr,
        TASK_PRIORITY_UI,
        &taskHandleUi,
        TASK_CORE_UI
    );
    if (result != pdPASS) {
        Serial.println("Failed to create UI task");
        return false;
    }

    // Create NVS task
    result = xTaskCreatePinnedToCore(
        taskNvs,
        "NVS",
        TASK_STACK_NVS,
        nullptr,
        TASK_PRIORITY_NVS,
        &taskHandleNvs,
        TASK_CORE_NVS
    );
    if (result != pdPASS) {
        Serial.println("Failed to create NVS task");
        return false;
    }

    Serial.println("\n=== Tasks Started ===");
    Serial.printf("  Pump:     Core %d, Priority %d, %dHz\n",
                  TASK_CORE_PUMP, TASK_PRIORITY_PUMP, 1000/PUMP_TASK_PERIOD_MS);
    Serial.printf("  SPI_Comm: Core %d, Priority %d, %dHz\n",
                  TASK_CORE_SPI_COMM, TASK_PRIORITY_SPI_COMM, 1000/SPI_TASK_PERIOD_MS);
    Serial.printf("  UI:       Core %d, Priority %d, %dHz\n",
                  TASK_CORE_UI, TASK_PRIORITY_UI, 1000/UI_TASK_PERIOD_MS);
    Serial.printf("  NVS:      Core %d, Priority %d, %dHz\n",
                  TASK_CORE_NVS, TASK_PRIORITY_NVS, 1000/NVS_TASK_PERIOD_MS);
    Serial.println("======================\n");

    return true;
}

TaskHandle_t getTaskPump() { return taskHandlePump; }
TaskHandle_t getTaskSpiComm() { return taskHandleSpiComm; }
TaskHandle_t getTaskUi() { return taskHandleUi; }
TaskHandle_t getTaskNvs() { return taskHandleNvs; }

// =============================================================================
// Thread-Safe State Access
// =============================================================================

uint16_t tasksGetCurrentRpm() {
    uint16_t rpm;
    if (STATE_LOCK()) {
        rpm = masterState.currentRpm;
        STATE_UNLOCK();
    } else {
        rpm = masterState.currentRpm;  // Volatile fallback
    }
    return rpm;
}

uint8_t tasksGetDisplayMode() {
    uint8_t mode;
    if (STATE_LOCK()) {
        mode = masterState.displayMode;
        STATE_UNLOCK();
    } else {
        mode = masterState.displayMode;
    }
    return mode;
}

uint16_t tasksGetManualRpm() {
    uint16_t rpm;
    if (STATE_LOCK()) {
        rpm = masterState.manualRpm;
        STATE_UNLOCK();
    } else {
        rpm = masterState.manualRpm;
    }
    return rpm;
}

SystemHealth tasksGetHealth() {
    return masterState.health;  // Atomic read
}

void tasksSetCurrentRpm(uint16_t rpm) {
    if (STATE_LOCK()) {
        masterState.currentRpm = rpm;
        STATE_UNLOCK();
    }
}

void tasksSetDisplayMode(uint8_t mode) {
    if (STATE_LOCK()) {
        if (masterState.displayMode != mode) {
            masterState.displayMode = mode;
            nvsSavePending = true;
            lastInputTime = millis();
        }
        STATE_UNLOCK();
    }
}

void tasksSetManualRpm(uint16_t rpm) {
    if (STATE_LOCK()) {
        if (masterState.manualRpm != rpm) {
            masterState.manualRpm = rpm;
            nvsSavePending = true;
            lastInputTime = millis();
        }
        STATE_UNLOCK();
    }
}

void tasksRequestNvsSave() {
    nvsSavePending = true;
    lastInputTime = millis();
}

void tasksEnterFailsafe(const char* reason) {
    if (masterState.health != HEALTH_FAILSAFE) {
        masterState.health = HEALTH_FAILSAFE;
        Serial.printf("!!! FAILSAFE: %s !!!\n", reason);

        // Log to SD
        if (sdIsReady()) {
            char entry[128];
            snprintf(entry, sizeof(entry), "%lu,FAILSAFE,%s\n", millis(), reason);
            sdAppendFileString("/crash_log.csv", entry);
        }
    }
}

void tasksExitFailsafe() {
    if (masterState.health == HEALTH_FAILSAFE) {
        masterState.health = HEALTH_OK;
        Serial.println("Failsafe cleared");
    }
}

// =============================================================================
// Pump Control Task - SAFETY CRITICAL
// =============================================================================
// Highest priority task, runs at 100Hz on Core 1
// Controls PWM output for pump motors
// Feeds watchdog - if this task hangs, system resets

static uint8_t rpmToPwmDuty(uint16_t rpm) {
    if (rpm >= 4000) return 255;
    return (uint8_t)((rpm * 255UL) / 4000UL);
}

static void taskPump(void* param) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(PUMP_TASK_PERIOD_MS);

    Serial.println("[Pump Task] Started - Safety Critical");

    while (true) {
        // Feed watchdog from this critical task
        esp_task_wdt_reset();

        // Update RTC uptime tracking (for crash diagnostics)
        static uint32_t lastUptimeUpdate = 0;
        uint32_t now = millis();
        if (now - lastUptimeUpdate >= 10000) {
            lastUptimeUpdate = now;
            rtcLastUptimeMs = now;
        }

        // Check for SPI timeout
        if ((now - masterState.lastValidSpiTime) > SPI_COMM_TIMEOUT_MS &&
            masterState.lastValidSpiTime > 0) {
            masterState.spiTimeoutCount++;
            tasksEnterFailsafe("SPI timeout");
        }

        // Calculate and set PWM
        uint8_t duty;
        if (masterState.health == HEALTH_FAILSAFE) {
            duty = FAILSAFE_PWM_DUTY;
        } else {
            uint16_t rpm = tasksGetCurrentRpm();
            duty = rpmToPwmDuty(rpm);
        }

        ledcWrite(PWM_OUTPUT_CHANNEL, duty);
        masterState.currentPwmDuty = duty;

        vTaskDelayUntil(&lastWakeTime, period);
    }
}

// =============================================================================
// SPI Communication Task
// =============================================================================
// Handles bidirectional SPI communication with slave display
// Sends RPM/mode to slave, receives UI commands back

static uint8_t lastSlaveMode = MODE_AUTO;
static uint16_t lastSlaveRpm = 3000;

static void taskSpiComm(void* param) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(SPI_TASK_PERIOD_MS);

    Serial.println("[SPI Task] Started");
    
    // Initialize OTA handler
    masterOtaInit();

    while (true) {
        uint32_t now = millis();
        
        // Check for OTA updates (this polls slave periodically)
        if (masterOtaProcess()) {
            // OTA is in progress - skip normal SPI exchange
            // Still need to maintain watchdog
            if (masterOtaGetState() == MASTER_OTA_COMPLETE && masterOtaRebootPending()) {
                // Update complete - reboot
                Serial.println("[SPI Task] OTA complete, rebooting...");
                vTaskDelay(pdMS_TO_TICKS(100));
                masterOtaReboot();
            }
            vTaskDelayUntil(&lastWakeTime, period);
            continue;
        }

        // Update simulation if in simulate mode
        if (masterState.opMode == OP_MODE_SIMULATE &&
            masterState.displayMode == MODE_AUTO) {

            if (now - masterState.lastSimChange >= SIM_CHANGE_INTERVAL_MS) {
                masterState.lastSimChange = now;

                if (STATE_LOCK()) {
                    if (masterState.simGoingUp) {
                        masterState.currentRpm = SIM_MAX_RPM;
                        masterState.simGoingUp = false;
                    } else {
                        masterState.currentRpm = SIM_MIN_RPM;
                        masterState.simGoingUp = true;
                    }
                    STATE_UNLOCK();
                }
            }
        } else if (masterState.displayMode == MODE_MANUAL) {
            // In manual mode, use manualRpm
            if (STATE_LOCK()) {
                masterState.currentRpm = masterState.manualRpm;
                STATE_UNLOCK();
            }
        }

        // Perform SPI exchange
        uint16_t rpmToSend = masterState.currentRpm;
        uint8_t modeToSend = masterState.displayMode;
        uint8_t reqMode;
        uint16_t reqRpm;

        if (spiExchange(rpmToSend, modeToSend, &reqMode, &reqRpm)) {
            // Valid response
            masterState.lastValidSpiTime = now;

            // Exit failsafe if we were in SPI timeout
            if (masterState.health == HEALTH_SPI_TIMEOUT ||
                masterState.health == HEALTH_FAILSAFE) {
                tasksExitFailsafe();
            }

            // Process slave UI requests
            bool changed = false;

            if (reqMode != lastSlaveMode) {
                lastSlaveMode = reqMode;
                if (reqMode != masterState.displayMode) {
                    tasksSetDisplayMode(reqMode);
                    changed = true;
                    Serial.printf("Mode -> %s (slave)\n",
                                  reqMode == MODE_AUTO ? "AUTO" : "MANUAL");
                }
            }

            if (reqRpm != lastSlaveRpm) {
                lastSlaveRpm = reqRpm;
                if (reqRpm != masterState.manualRpm) {
                    tasksSetManualRpm(reqRpm);
                    changed = true;
                    Serial.printf("RPM -> %u (slave)\n", reqRpm);
                }
            }
        } else {
            // SPI failed
            if (masterState.health == HEALTH_OK) {
                masterState.health = HEALTH_SPI_TIMEOUT;
            }
        }

        vTaskDelayUntil(&lastWakeTime, period);
    }
}

// =============================================================================
// UI Task - Encoder and Serial
// =============================================================================

// lastEncoderClk removed - now using MCP23017-based encoder_mux
static uint32_t lastButtonPress = 0;
#define BUTTON_DEBOUNCE_MS 200

static const char* getOpModeName(OperatingMode m) {
    switch (m) {
        case OP_MODE_SNIFF:    return "SNIFF";
        case OP_MODE_RPM:      return "RPM";
        case OP_MODE_SIMULATE: return "SIMULATE";
        default:               return "UNKNOWN";
    }
}

static const char* getHealthName(SystemHealth h) {
    switch (h) {
        case HEALTH_OK:          return "OK";
        case HEALTH_SPI_TIMEOUT: return "SPI_TIMEOUT";
        case HEALTH_CAN_ERROR:   return "CAN_ERROR";
        case HEALTH_FAILSAFE:    return "FAILSAFE";
        default:                 return "UNKNOWN";
    }
}

static void printHelp() {
    Serial.println("\n=== CAN-to-SPI Master (FreeRTOS) ===");
    Serial.println("Commands:");
    Serial.println("  s - Sniff mode");
    Serial.println("  r - RPM extraction mode");
    Serial.println("  m - Simulation mode");
    Serial.println("  t - Send test RPM");
    Serial.println("  c - Statistics");
    Serial.println("  h - System health");
    Serial.println("  T - Task info");
    Serial.println("RPM Pulse Counter:");
    Serial.println("  p - Enable / show RPM reading");
    Serial.println("  P - Disable pulse counter");
    Serial.println("SD Card:");
    Serial.println("  d - List root directory");
    Serial.println("  x - SD card info");
    Serial.println("  L - View crash log");
    Serial.println("  ? - This help");
    Serial.println();
}

static void printStats() {
    Serial.println("\n=== Statistics ===");
    Serial.printf("CAN Messages: %lu\n", canGetMessageCount());
    Serial.printf("CAN Errors: %lu\n", canGetErrorCount());
    Serial.printf("SPI Success: %lu\n", spiGetSuccessCount());
    Serial.printf("SPI Errors: %lu\n", spiGetErrorCount());
    Serial.printf("SPI Timeouts: %lu\n", masterState.spiTimeoutCount);
    Serial.printf("Current RPM: %u\n", masterState.currentRpm);
    Serial.printf("Current PWM: %u\n", masterState.currentPwmDuty);
    Serial.printf("Op Mode: %s\n", getOpModeName(masterState.opMode));
    Serial.printf("Display Mode: %s\n", masterState.displayMode == MODE_AUTO ? "AUTO" : "MANUAL");
    Serial.printf("Manual RPM: %u\n", masterState.manualRpm);
    Serial.printf("Health: %s\n", getHealthName(masterState.health));
    Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
    Serial.printf("Resets: %lu (WDT: %lu)\n", rtcResetCount, rtcWdtResetCount);
    Serial.println();
}

static void printTaskInfo() {
    Serial.println("\n=== Task Info ===");
    Serial.printf("Pump:     stack=%u, state=%d\n",
                  uxTaskGetStackHighWaterMark(taskHandlePump),
                  eTaskGetState(taskHandlePump));
    Serial.printf("SPI_Comm: stack=%u, state=%d\n",
                  uxTaskGetStackHighWaterMark(taskHandleSpiComm),
                  eTaskGetState(taskHandleSpiComm));
    Serial.printf("UI:       stack=%u, state=%d\n",
                  uxTaskGetStackHighWaterMark(taskHandleUi),
                  eTaskGetState(taskHandleUi));
    Serial.printf("NVS:      stack=%u, state=%d\n",
                  uxTaskGetStackHighWaterMark(taskHandleNvs),
                  eTaskGetState(taskHandleNvs));
    Serial.println();
}

static void processEncoder() {
    // Update MCP23017-based encoder state
    if (!encoderMuxIsEnabled()) {
        return;
    }

    // Update all encoders via I2C
    encoderMuxUpdate();

    // Check power steering encoder button for mode toggle
    if (encoderMuxButtonPressed(ENCODER_POWER_STEERING)) {
        uint8_t mode = tasksGetDisplayMode();
        uint8_t newMode = (mode == MODE_AUTO) ? MODE_MANUAL : MODE_AUTO;
        tasksSetDisplayMode(newMode);
        Serial.printf("Button: %s\n", newMode == MODE_AUTO ? "AUTO" : "MANUAL");
    }

    // Power steering level is handled automatically by encoder_mux
    // Get it when needed via encoderMuxGetPowerSteeringLevel()
}

static void processSerial() {
    if (!Serial.available()) return;

    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() == 0) return;

    char cmd = input[0];

    switch (cmd) {
        case 's':
        case 'S':
            masterState.opMode = OP_MODE_SNIFF;
            canSetMode(CAN_MODE_SNIFF);
            Serial.println("Sniff mode");
            break;

        case 'r':
        case 'R':
            masterState.opMode = OP_MODE_RPM;
            canSetMode(CAN_MODE_RPM);
            Serial.println("RPM mode");
            break;

        case 'm':
        case 'M':
            masterState.opMode = OP_MODE_SIMULATE;
            masterState.lastSimChange = millis();
            masterState.simGoingUp = true;
            tasksSetCurrentRpm(SIM_MIN_RPM);
            Serial.println("Simulate mode");
            break;

        case 't':
        case 'T':
            if (cmd == 'T') {
                printTaskInfo();
            } else {
                uint8_t reqMode;
                uint16_t reqRpm;
                if (spiExchange(1234, masterState.displayMode, &reqMode, &reqRpm)) {
                    Serial.printf("Test OK: slave req mode=%s, rpm=%u\n",
                                  reqMode == MODE_AUTO ? "AUTO" : "MANUAL", reqRpm);
                } else {
                    Serial.println("Test failed");
                }
            }
            break;

        case 'c':
        case 'C':
            printStats();
            break;

        case 'h':
        case 'H':
            Serial.printf("\nHealth: %s\n", getHealthName(masterState.health));
            Serial.printf("Last SPI: %lu ms ago\n", millis() - masterState.lastValidSpiTime);
            Serial.printf("Failsafe PWM: %d\n", FAILSAFE_PWM_DUTY);
            Serial.printf("WDT timeout: %d sec\n", WDT_TIMEOUT_SEC);
            Serial.println();
            break;

        case 'd':
            sdPrintDir("/", 2);
            break;

        case 'x':
            if (sdIsReady()) {
                Serial.printf("\nSD: %s, %lluMB total, %lluMB free\n",
                              sdGetCardType(),
                              sdGetTotalBytes() / (1024*1024),
                              sdGetFreeBytes() / (1024*1024));
            } else {
                Serial.println("SD not mounted");
            }
            break;

        case 'L':
            {
                String log = sdReadFileString("/crash_log.csv");
                if (log.length() > 0) {
                    Serial.println("=== Crash Log ===");
                    Serial.println(log);
                    Serial.println("=================");
                } else {
                    Serial.println("No crash log");
                }
            }
            break;

        case 'p':
            // Enable RPM pulse counter
            if (!rpmCounterIsEnabled()) {
                rpmCounterEnable();
                Serial.println("RPM counter enabled");
            } else {
                // Show RPM reading
                Serial.printf("RPM: %.0f (total pulses: %lu)\n",
                              rpmCounterGetRPM(),
                              rpmCounterGetTotalPulses());
            }
            break;

        case 'P':
            // Disable RPM pulse counter
            if (rpmCounterIsEnabled()) {
                rpmCounterDisable();
                Serial.println("RPM counter disabled");
            } else {
                Serial.println("RPM counter already disabled");
            }
            break;

        case '?':
            printHelp();
            break;

        default:
            Serial.printf("Unknown: %c\n", cmd);
            break;
    }
}

static void taskUi(void* param) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(UI_TASK_PERIOD_MS);

    // Initialize MCP23017-based encoder multiplexer
    if (encoderMuxInit()) {
        encoderMuxEnable();
        Serial.println("[UI Task] Encoder MUX initialized");
    } else {
        Serial.println("[UI Task] WARNING: Encoder MUX init failed - MCP23017 not found");
    }

    Serial.println("[UI Task] Started");

    // Heartbeat counter
    uint32_t loopCount = 0;

    while (true) {
        processEncoder();
        processSerial();

        // Heartbeat every 5 seconds
        loopCount++;
        if (loopCount >= (5000 / UI_TASK_PERIOD_MS)) {
            loopCount = 0;
            Serial.printf("Heartbeat: %s, rpm=%u, pwm=%u\n",
                          getHealthName(masterState.health),
                          masterState.currentRpm,
                          masterState.currentPwmDuty);
        }

        vTaskDelayUntil(&lastWakeTime, period);
    }
}

// =============================================================================
// NVS Task - Settings Persistence
// =============================================================================

static void taskNvs(void* param) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(NVS_TASK_PERIOD_MS);

    Serial.println("[NVS Task] Started");

    while (true) {
        // Check if save is pending and debounce time has passed
        if (nvsSavePending) {
            uint32_t now = millis();
            if ((now - lastInputTime) >= NVS_SAVE_DEBOUNCE_MS) {
                // Check if values actually changed
                bool modeChanged = (masterState.displayMode != savedDisplayMode);
                bool rpmChanged = (masterState.manualRpm != savedManualRpm);

                if (modeChanged || rpmChanged) {
                    prefs.begin("master", false);
                    if (modeChanged) {
                        prefs.putUChar("mode", masterState.displayMode);
                        savedDisplayMode = masterState.displayMode;
                    }
                    if (rpmChanged) {
                        prefs.putUShort("manualRpm", masterState.manualRpm);
                        savedManualRpm = masterState.manualRpm;
                    }
                    prefs.end();

                    Serial.printf("NVS saved: mode=%s, rpm=%u\n",
                                  masterState.displayMode == MODE_AUTO ? "AUTO" : "MANUAL",
                                  masterState.manualRpm);
                }

                nvsSavePending = false;
            }
        }

        vTaskDelayUntil(&lastWakeTime, period);
    }
}
