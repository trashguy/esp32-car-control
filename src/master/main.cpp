#include <Arduino.h>
#include <esp_task_wdt.h>
#include "can_handler.h"
#include "i2c_master.h"
#include "shared/config.h"
#include "shared/protocol.h"

// Watchdog timeout in seconds
#define WDT_TIMEOUT_SEC 5

// Configuration - modify these after identifying the correct CAN message
static const uint32_t RPM_CAN_ID = 0x000;     // Set after sniffing
static const uint8_t RPM_BYTE_OFFSET = 0;     // Byte position in CAN data
static const float RPM_SCALE = 1.0f;          // Multiplier for raw value

// Operating modes
enum OperatingMode {
    MODE_SNIFF,      // Log all CAN messages
    MODE_RPM,        // Extract RPM from CAN
    MODE_SIMULATE    // Generate simulated RPM
};

// State
static uint16_t currentRpm = 0;
static unsigned long lastSendTime = 0;
static OperatingMode currentMode = MODE_SIMULATE;  // Start in simulation mode

// Manual mode static RPM
#define MANUAL_STATIC_RPM 3000

// Auto mode simulation - simple range 3500-4500
#define SIM_MIN_RPM 3500
#define SIM_MAX_RPM 4500
static unsigned long lastSimChange = 0;
static bool simGoingUp = true;

// I2C communication state
static uint16_t lastSentRpm = 0;
static unsigned long lastKeepalive = 0;

// Timing constants
#define SIM_CHANGE_INTERVAL_MS  1000   // Change simulated RPM every 1 second
#define KEEPALIVE_INTERVAL_MS   2000   // Send keepalive every 2 seconds even if RPM unchanged

// Generate next simulated RPM value - simple alternating between min and max
uint16_t generateNextSimulatedRpm() {
    if (simGoingUp) {
        simGoingUp = false;
        return SIM_MAX_RPM;
    } else {
        simGoingUp = true;
        return SIM_MIN_RPM;
    }
}

const char* getModeName(OperatingMode m) {
    switch (m) {
        case MODE_SNIFF:    return "SNIFF";
        case MODE_RPM:      return "RPM";
        case MODE_SIMULATE: return "SIMULATE";
        default:            return "UNKNOWN";
    }
}

void printHelp() {
    Serial.println("\n=== CAN-to-I2C Master ===");
    Serial.println("Commands:");
    Serial.println("  s - Sniff mode (log CAN messages)");
    Serial.println("  r - RPM extraction mode");
    Serial.println("  m - Simulation mode (no CAN needed)");
    Serial.println("  i<hex> - Set RPM message ID (e.g., i1A0)");
    Serial.println("  o<num> - Set byte offset (e.g., o2)");
    Serial.println("  t - Send test RPM (1234)");
    Serial.println("  c - Show statistics");
    Serial.println("  ? - Show this help");
    Serial.println();
}

void printStats() {
    Serial.println("\n=== Statistics ===");
    Serial.printf("CAN Messages: %lu\n", canGetMessageCount());
    Serial.printf("CAN Errors: %lu\n", canGetErrorCount());
    Serial.printf("I2C Success: %lu\n", i2cGetSuccessCount());
    Serial.printf("I2C Errors: %lu\n", i2cGetErrorCount());
    Serial.printf("Current RPM: %u\n", currentRpm);
    Serial.printf("Last Sent RPM: %u\n", lastSentRpm);
    Serial.printf("Mode: %s\n", getModeName(currentMode));
    if (currentMode == MODE_SIMULATE) {
        Serial.printf("Sim Range: %u - %u RPM\n", SIM_MIN_RPM, SIM_MAX_RPM);
        Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    }
    Serial.println();
}

void processSerialCommand() {
    if (!Serial.available()) return;

    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() == 0) return;

    char cmd = input[0];
    String param = input.substring(1);

    switch (cmd) {
        case 's':
        case 'S':
            currentMode = MODE_SNIFF;
            canSetMode(CAN_MODE_SNIFF);
            Serial.println("Sniff mode enabled");
            break;

        case 'r':
        case 'R':
            currentMode = MODE_RPM;
            canSetMode(CAN_MODE_RPM);
            Serial.println("RPM extraction mode enabled");
            break;

        case 'm':
        case 'M':
            currentMode = MODE_SIMULATE;
            lastSimChange = millis();  // Reset simulation timer
            simGoingUp = true;         // Reset direction
            currentRpm = SIM_MIN_RPM;  // Start at min
            Serial.println("Simulation mode enabled");
            Serial.printf("Range: %u-%u RPM, Change every %d ms\n",
                         SIM_MIN_RPM, SIM_MAX_RPM, SIM_CHANGE_INTERVAL_MS);
            break;

        case 'i':
        case 'I':
            if (param.length() > 0) {
                uint32_t id = strtoul(param.c_str(), nullptr, 16);
                canSetRpmMessageId(id);
                Serial.printf("RPM message ID set to 0x%03X\n", id);
            }
            break;

        case 'o':
        case 'O':
            if (param.length() > 0) {
                uint8_t offset = param.toInt();
                canSetRpmExtraction(offset, RPM_SCALE);
                Serial.printf("RPM byte offset set to %d\n", offset);
            }
            break;

        case 't':
        case 'T':
            {
                uint16_t testRpm = 1234;
                if (i2cSendRpm(testRpm)) {
                    Serial.printf("Sent test RPM: %u\n", testRpm);
                } else {
                    Serial.println("Failed to send test RPM");
                }
            }
            break;

        case 'c':
        case 'C':
            printStats();
            break;

        case '?':
        case 'h':
        case 'H':
            printHelp();
            break;

        default:
            Serial.printf("Unknown command: %c\n", cmd);
            printHelp();
            break;
    }
}

void setup() {
    Serial.begin(115200);

    // Wait for USB CDC to be ready (ESP32-S3 specific) with timeout
    unsigned long start = millis();
    while (!Serial && (millis() - start) < 3000) {
        delay(10);
    }
    delay(100);

    Serial.println("\n\n========================================");
    Serial.println("  ESP32-S3 CAN-to-I2C Master");
    Serial.println("========================================\n");

    Serial.printf("CPU Freq: %d MHz\n", getCpuFrequencyMhz());
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());

    // Initialize hardware watchdog timer
    esp_task_wdt_init(WDT_TIMEOUT_SEC, true);  // timeout in seconds, panic on timeout
    esp_task_wdt_add(NULL);  // Add current task to watchdog
    Serial.printf("Watchdog enabled: %d second timeout\n", WDT_TIMEOUT_SEC);

    // Initialize I2C master
    if (!i2cMasterInit()) {
        Serial.println("I2C initialization failed!");
    }

    // Initialize CAN controller
    if (!canInit()) {
        Serial.println("CAN initialization failed!");
        Serial.println("Check MCP2515 wiring and power");
    }

    // Configure CAN (even if starting in sim mode)
    canSetMode(CAN_MODE_SNIFF);
    canSetRpmMessageId(RPM_CAN_ID);
    canSetRpmExtraction(RPM_BYTE_OFFSET, RPM_SCALE);

    // Initialize simulation and keepalive timers
    lastSimChange = millis();
    lastKeepalive = millis();
    currentRpm = MANUAL_STATIC_RPM;  // Start with manual mode RPM
    lastSentRpm = 0;  // Force initial send

    Serial.println("Starting in SIMULATION mode - sending RPM over I2C");
    Serial.printf("Simulation range: %u-%u RPM (alternates every %d ms)\n",
                  SIM_MIN_RPM, SIM_MAX_RPM, SIM_CHANGE_INTERVAL_MS);
    Serial.println("Slave controls display mode (Auto/Manual)");
    printHelp();
}

void loop() {
    // Reset hardware watchdog
    esp_task_wdt_reset();

    // Process serial commands
    processSerialCommand();

    unsigned long now = millis();

    // Process based on current mode
    switch (currentMode) {
        case MODE_SNIFF:
            // Just process CAN messages (they get printed in canProcess)
            canProcess(nullptr);
            break;

        case MODE_RPM: {
            // Extract RPM from CAN and send over I2C
            uint16_t rpm;
            if (canProcess(&rpm)) {
                currentRpm = rpm;
            }
            now = millis();
            if (now - lastSendTime >= I2C_SEND_INTERVAL_MS) {
                lastSendTime = now;
                i2cSendRpm(currentRpm);
            }
            break;
        }

        case MODE_SIMULATE: {
            now = millis();

            // Alternate between 3500 and 4500 every second
            // Slave decides what to display based on its local mode setting
            if (now - lastSimChange >= SIM_CHANGE_INTERVAL_MS) {
                lastSimChange = now;
                currentRpm = generateNextSimulatedRpm();
            }

            // Send I2C only if RPM changed OR keepalive interval expired
            bool shouldSend = false;
            if (currentRpm != lastSentRpm) {
                shouldSend = true;  // RPM changed, send immediately
            } else if (now - lastKeepalive >= KEEPALIVE_INTERVAL_MS) {
                shouldSend = true;  // Keepalive - maintain connection
            }

            if (shouldSend) {
                if (i2cSendRpm(currentRpm)) {
                    lastSentRpm = currentRpm;
                    lastKeepalive = now;
                }
            }
            break;
        }
    }

    // Small delay to prevent tight loop
    delay(1);
}
