#include <Arduino.h>
#include "i2c_slave.h"
#include "display.h"
#include "sd_card.h"
#include "shared/config.h"

// State
static volatile bool newRpmAvailable = false;
static volatile uint16_t latestRpm = 0;

// Callback when RPM is received via I2C
void onRpmReceived(uint16_t rpm) {
    latestRpm = rpm;
    newRpmAvailable = true;
}

void printStats() {
    Serial.println("\n=== Slave Statistics ===");
    Serial.printf("Valid packets: %lu\n", i2cGetValidPacketCount());
    Serial.printf("Invalid packets: %lu\n", i2cGetInvalidPacketCount());
    Serial.printf("Last RPM: %u\n", i2cGetLastRpm());
    Serial.printf("Connected: %s\n", i2cIsConnected() ? "YES" : "NO");
    Serial.printf("Time since last packet: %lu ms\n", i2cGetTimeSinceLastPacket());
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
            Serial.println("\n=== I2C Display Slave ===");
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
    Serial.println("  ESP32-S3 I2C Display Slave");
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

    // Initialize I2C slave on Wire1 (separate from touch on Wire)
    if (!i2cSlaveInit(onRpmReceived)) {
        Serial.println("I2C slave initialization failed!");
    }

    Serial.println("\nSlave ready. Waiting for RPM data...\n");
}

void loop() {
    // Process serial commands
    processSerialCommand();

    // Update display with new RPM if available
    if (newRpmAvailable) {
        newRpmAvailable = false;
        displayUpdateRpm(latestRpm);
    }

    // Check connection status
    bool connected = i2cIsConnected();
    displaySetConnected(connected);

    // Process display animations
    displayLoop();

    // Small delay to prevent busy-looping
    delay(10);
}
