#include <Arduino.h>
#include "slave/spi_slave.h"
#include "display.h"
#include "sd_card.h"
#include "shared/config.h"
#include "shared/protocol.h"

// State
static volatile bool newDataAvailable = false;
static volatile uint16_t latestRpm = 0;
static volatile uint8_t latestMode = MODE_AUTO;

// Callback when data is received from master via SPI
void onMasterData(uint16_t rpm, uint8_t mode) {
    // Flag as new if anything changed
    if (rpm != latestRpm || mode != latestMode) {
        latestRpm = rpm;
        latestMode = mode;
        newDataAvailable = true;
    }
}

void printStats() {
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
}

void processSerialCommand() {
    if (!Serial.available()) return;

    char cmd = Serial.read();
    // Drain any remaining characters
    while (Serial.available()) Serial.read();

    switch (cmd) {
        case 'c':
        case 'C':
            printStats();
            break;

        case '?':
        case 'h':
        case 'H':
            Serial.println("\n=== SPI Display Slave ===");
            Serial.println("Commands:");
            Serial.println("  c - Show statistics");
            Serial.println("  ? - Show this help");
            Serial.println();
            break;

        default:
            break;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n========================================");
    Serial.println("  ESP32-S3 SPI Display Slave");
    Serial.println("========================================\n");

    Serial.printf("CPU Freq: %d MHz\n", getCpuFrequencyMhz());
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());

    // Initialize SD card first (before display takes SPI)
    if (!sdCardInit()) {
        Serial.println("SD card not available (continuing without it)");
    }

    // Initialize display (also initializes Wire for touch)
    if (!displayInit()) {
        Serial.println("Display initialization failed!");
    }

    // Initialize SPI slave for master communication
    if (!spiSlaveInit(onMasterData)) {
        Serial.println("SPI slave initialization failed!");
    }

    Serial.println("\nSlave ready. Master is source of truth.\n");
}

void loop() {
    // Process serial commands
    processSerialCommand();

    // Process SPI slave transactions
    spiSlaveProcess();

    // Check if we just reconnected - force display refresh even if values unchanged
    if (spiSlaveCheckReconnected()) {
        latestRpm = spiSlaveGetLastRpm();
        latestMode = spiSlaveGetMasterMode();
        newDataAvailable = true;
    }

    // Update display with data from master
    if (newDataAvailable) {
        newDataAvailable = false;
        displayUpdateRpm(latestRpm);
    }

    // Check connection status
    bool connected = spiSlaveIsConnected();
    displaySetConnected(connected);

    // Process display animations
    displayLoop();

    // Small delay to prevent busy-looping
    delay(1);
}
