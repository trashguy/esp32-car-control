#include <Arduino.h>
#include "slave/spi_slave.h"
#include "slave/ota_handler.h"
#include "display.h"
#include "tasks.h"
#include "sd_card.h"
#include "usb_msc.h"
#include "shared/config.h"
#include "shared/protocol.h"
#include "shared/version.h"

// =============================================================================
// FreeRTOS Task-Based Architecture
// =============================================================================
//
// This slave device uses FreeRTOS tasks for concurrent operation:
//
// Task           Priority  Core  Purpose
// ------------   --------  ----  ----------------------------------------
// SPI_Comm       5 (High)  1     Master communication (100Hz)
// Display        3 (Med)   1     UI rendering and touch (~60Hz)
// Serial         1 (Low)   0     Debug commands
//
// Inter-task communication:
// - queueSpiToDisplay: RPM/mode updates from master
// - queueDisplayToSpi: User commands to master
//
// The Arduino loop() is not used - all work happens in tasks.
// =============================================================================

// Callback when data is received from master via SPI
// Note: This runs in the context of spiSlaveProcess(), which is called from SPI task
static void onMasterData(uint16_t rpm, uint8_t mode) {
    // Data handling is now done in the SPI task via queues
    // This callback is kept for compatibility but the task handles updates
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n========================================");
    Serial.println("  ESP32-S3 SPI Display Slave (FreeRTOS)");
    Serial.printf("  Version: %s\n", FIRMWARE_VERSION);
    Serial.printf("  Built: %s\n", BUILD_TIMESTAMP);
    Serial.println("========================================\n");

    Serial.printf("CPU Freq: %d MHz\n", getCpuFrequencyMhz());
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("Cores available: %d\n", ESP.getChipCores());

    // Initialize FreeRTOS objects (queues, mutexes)
    if (!tasksInit()) {
        Serial.println("FATAL: Failed to initialize FreeRTOS objects!");
        while (1) delay(1000);
    }

    // Initialize SD card first (before display takes SPI)
    if (!sdCardInit()) {
        Serial.println("SD card not available (continuing without it)");
    }
#if PRODUCTION_BUILD
    else {
        // Initialize USB Mass Storage (disabled by default)
        // Enable via Settings screen USB button to expose SD card to PC
        if (!usbMscInit()) {
            Serial.println("USB MSC initialization failed (continuing without it)");
        }
    }
#endif

    // Initialize display (also initializes Wire for touch)
    if (!displayInit()) {
        Serial.println("Display initialization failed!");
    }

    // Initialize SPI slave for master communication
    if (!spiSlaveInit(onMasterData)) {
        Serial.println("SPI slave initialization failed!");
    }

    Serial.println("\nHardware initialized. Starting FreeRTOS tasks...\n");

    // Start all tasks
    if (!tasksStart()) {
        Serial.println("FATAL: Failed to start tasks!");
        while (1) delay(1000);
    }

    Serial.println("All tasks started. Slave ready.\n");
#if PRODUCTION_BUILD
    Serial.println("Commands: 'c' = stats, 't' = task info, 'e' = eject USB, '?' = help\n");
#else
    Serial.println("Commands: 'c' = stats, 't' = task info, '?' = help\n");
#endif
}

void loop() {
    // All work is done in FreeRTOS tasks
    // This loop runs at lowest priority and just yields
    vTaskDelay(pdMS_TO_TICKS(1000));
}
