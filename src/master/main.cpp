#include <Arduino.h>
#include <esp_task_wdt.h>
#include "tasks.h"
#include "master/spi_master.h"
#include "master/sd_handler.h"
#include "can_handler.h"
#include "rpm_counter.h"
#include "shared/config.h"
#include "shared/protocol.h"
#include "shared/version.h"

#if VIRTUAL_MEMORY
#include "master/virtual_memory.h"
#endif

// =============================================================================
// FreeRTOS Task-Based Architecture
// =============================================================================
//
// This master device uses FreeRTOS tasks for concurrent operation:
//
// Task        Priority  Core  Rate    Purpose
// ----------  --------  ----  ------  ----------------------------------
// Pump        10 (Max)  1     100Hz   Safety-critical PWM output
// SPI_Comm    5         0     10Hz    Slave communication
// UI          3         1     50Hz    Encoder input, serial commands
// NVS         1         0     1Hz     Debounced settings persistence
//
// Safety features:
// - Watchdog fed from Pump task (highest priority)
// - Failsafe mode on SPI timeout (pumps run at safe speed)
// - RTC memory tracks crashes across resets
// - Crash log written to SD card
//
// =============================================================================

void setup() {
    Serial.begin(115200);

    // Wait for USB CDC with timeout
    unsigned long start = millis();
    while (!Serial && (millis() - start) < 3000) {
        delay(10);
    }
    delay(100);

    Serial.println("\n\n========================================");
    Serial.println("  ESP32-S3 CAN-to-SPI Master (FreeRTOS)");
    Serial.println("  SAFETY-CRITICAL BUILD");
    Serial.printf("  Version: %s\n", FIRMWARE_VERSION);
    Serial.printf("  Built: %s\n", BUILD_TIMESTAMP);
    Serial.println("========================================\n");

    Serial.printf("CPU: %d MHz, Heap: %d bytes\n",
                  getCpuFrequencyMhz(), ESP.getFreeHeap());

    // Initialize PWM for pump control FIRST (before anything else)
    // This ensures pumps can be controlled even if other init fails
    ledcSetup(PWM_OUTPUT_CHANNEL, PWM_OUTPUT_FREQ, PWM_OUTPUT_RESOLUTION);
    ledcAttachPin(PWM_OUTPUT_PIN, PWM_OUTPUT_CHANNEL);
    ledcWrite(PWM_OUTPUT_CHANNEL, 0);  // Start with pumps off
    Serial.printf("PWM: GPIO %d, %dHz, %d-bit\n",
                  PWM_OUTPUT_PIN, PWM_OUTPUT_FREQ, PWM_OUTPUT_RESOLUTION);

    // Initialize watchdog - CRITICAL for safety
    esp_task_wdt_init(WDT_TIMEOUT_SEC, true);  // Panic on timeout
    esp_task_wdt_add(NULL);  // Add setup task
    Serial.printf("Watchdog: %d sec timeout\n", WDT_TIMEOUT_SEC);

    // Initialize FreeRTOS objects (queues, mutexes, load settings)
    if (!tasksInit()) {
        Serial.println("FATAL: Failed to init tasks!");
        while (1) { esp_task_wdt_reset(); delay(1000); }
    }

    // Initialize SPI master for slave communication
    if (!spiMasterInit()) {
        Serial.println("WARNING: SPI init failed");
    }

    // Initialize SD card
    if (!sdInit()) {
        Serial.println("SD card not available");
    } else {
        // Log crash info to SD if available
        if (sdIsReady()) {
            esp_reset_reason_t reason = esp_reset_reason();
            if (reason == ESP_RST_TASK_WDT || reason == ESP_RST_INT_WDT ||
                reason == ESP_RST_WDT || reason == ESP_RST_PANIC) {
                char entry[64];
                snprintf(entry, sizeof(entry), "%lu,BOOT_AFTER_CRASH,%d\n",
                         millis(), (int)reason);
                sdAppendFileString("/crash_log.csv", entry);
            }
        }

        #if VIRTUAL_MEMORY
        // Initialize virtual memory system (SD-backed PSRAM cache)
        if (sdIsReady()) {
            Serial.println("\nInitializing virtual memory...");
            if (vmem.init()) {
                Serial.printf("Virtual memory ready: %lu MB virtual, %.1f%% cache\n",
                              vmem.getTotalSize() / (1024 * 1024),
                              100.0f * vmem.getCacheSize() / vmem.getTotalSize());
            } else {
                Serial.println("WARNING: Virtual memory init failed");
            }
        }
        #endif
    }

    // CAN disabled for now
    // canInit();

    // Initialize RPM counter (starts disabled)
    // Enable with 'r' command, disable with 'R' command
    if (rpmCounterInit()) {
        Serial.printf("RPM counter: GPIO %d (disabled, use 'r' to enable)\n", RPM_INPUT_PIN);
    } else {
        Serial.println("WARNING: RPM counter init failed");
    }

    Serial.println("\nStarting tasks...\n");

    // Start all FreeRTOS tasks
    if (!tasksStart()) {
        Serial.println("FATAL: Failed to start tasks!");
        while (1) { esp_task_wdt_reset(); delay(1000); }
    }

    // Remove setup task from watchdog (pump task now handles it)
    esp_task_wdt_delete(NULL);

    Serial.println("=== SAFETY FEATURES ===");
    Serial.printf("  Watchdog: %d sec\n", WDT_TIMEOUT_SEC);
    Serial.printf("  SPI timeout: %d ms -> failsafe\n", SPI_COMM_TIMEOUT_MS);
    Serial.printf("  Failsafe PWM: %d (%.0f%%)\n",
                  FAILSAFE_PWM_DUTY, FAILSAFE_PWM_DUTY * 100.0 / 255.0);
    Serial.println("=======================\n");

    Serial.println("Commands: c=stats, h=health, T=tasks, p/P=pulse rpm on/off, ?=help\n");
}

void loop() {
    // All work is done in FreeRTOS tasks
    // Main loop just yields to scheduler
    vTaskDelay(pdMS_TO_TICKS(1000));
}
