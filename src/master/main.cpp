#include <Arduino.h>
#include "can_handler.h"
#include "i2c_master.h"
#include "shared/config.h"

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

// Simulation patterns
enum SimPattern {
    SIM_SINE,        // Smooth sine wave
    SIM_RAMP,        // Linear ramp up/down
    SIM_RANDOM,      // Random values
    SIM_STEP         // Step changes
};

// State
static uint16_t currentRpm = 0;
static unsigned long lastSendTime = 0;
static OperatingMode currentMode = MODE_SNIFF;

// Simulation state
static SimPattern simPattern = SIM_SINE;
static uint16_t simMinRpm = 800;
static uint16_t simMaxRpm = 6000;
static uint16_t simPeriodMs = 4000;  // Full cycle period
static unsigned long simStartTime = 0;

// Generate simulated RPM based on current pattern
uint16_t generateSimulatedRpm() {
    unsigned long elapsed = millis() - simStartTime;
    float phase = (float)(elapsed % simPeriodMs) / simPeriodMs;  // 0.0 to 1.0
    uint16_t range = simMaxRpm - simMinRpm;

    switch (simPattern) {
        case SIM_SINE: {
            // Smooth sine wave oscillation
            float sineVal = (sin(phase * 2.0 * PI) + 1.0) / 2.0;  // 0.0 to 1.0
            return simMinRpm + (uint16_t)(sineVal * range);
        }

        case SIM_RAMP: {
            // Triangle wave - ramp up then down
            float rampVal = (phase < 0.5) ? (phase * 2.0) : (2.0 - phase * 2.0);
            return simMinRpm + (uint16_t)(rampVal * range);
        }

        case SIM_RANDOM: {
            // Random value changes every 500ms
            static uint16_t lastRandom = simMinRpm;
            static unsigned long lastRandomTime = 0;
            if (millis() - lastRandomTime > 500) {
                lastRandomTime = millis();
                lastRandom = simMinRpm + (random(range));
            }
            return lastRandom;
        }

        case SIM_STEP: {
            // Step through discrete values
            int steps = 5;
            int stepIndex = (int)(phase * steps) % steps;
            return simMinRpm + (range * stepIndex / (steps - 1));
        }

        default:
            return simMinRpm;
    }
}

const char* getPatternName(SimPattern p) {
    switch (p) {
        case SIM_SINE:   return "SINE";
        case SIM_RAMP:   return "RAMP";
        case SIM_RANDOM: return "RANDOM";
        case SIM_STEP:   return "STEP";
        default:         return "UNKNOWN";
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
    Serial.println("  p<0-3> - Set sim pattern (0=sine,1=ramp,2=random,3=step)");
    Serial.println("  n<num> - Set sim min RPM (e.g., n800)");
    Serial.println("  x<num> - Set sim max RPM (e.g., x6000)");
    Serial.println("  d<num> - Set sim period ms (e.g., d4000)");
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
    Serial.printf("Mode: %s\n", getModeName(currentMode));
    if (currentMode == MODE_SIMULATE) {
        Serial.printf("Sim Pattern: %s\n", getPatternName(simPattern));
        Serial.printf("Sim Range: %u - %u RPM\n", simMinRpm, simMaxRpm);
        Serial.printf("Sim Period: %u ms\n", simPeriodMs);
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
            simStartTime = millis();
            Serial.println("Simulation mode enabled");
            Serial.printf("Pattern: %s, Range: %u-%u RPM, Period: %u ms\n",
                         getPatternName(simPattern), simMinRpm, simMaxRpm, simPeriodMs);
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

        case 'p':
        case 'P':
            if (param.length() > 0) {
                int p = param.toInt();
                if (p >= 0 && p <= 3) {
                    simPattern = (SimPattern)p;
                    Serial.printf("Sim pattern set to %s\n", getPatternName(simPattern));
                }
            }
            break;

        case 'n':
        case 'N':
            if (param.length() > 0) {
                simMinRpm = param.toInt();
                Serial.printf("Sim min RPM set to %u\n", simMinRpm);
            }
            break;

        case 'x':
        case 'X':
            if (param.length() > 0) {
                simMaxRpm = param.toInt();
                Serial.printf("Sim max RPM set to %u\n", simMaxRpm);
            }
            break;

        case 'd':
        case 'D':
            if (param.length() > 0) {
                simPeriodMs = param.toInt();
                Serial.printf("Sim period set to %u ms\n", simPeriodMs);
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
    delay(1000);

    Serial.println("\n\n========================================");
    Serial.println("  ESP32-S3 CAN-to-I2C Master");
    Serial.println("========================================\n");

    Serial.printf("CPU Freq: %d MHz\n", getCpuFrequencyMhz());
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());

    // Initialize I2C master
    if (!i2cMasterInit()) {
        Serial.println("I2C initialization failed!");
    }

    // Initialize CAN controller
    if (!canInit()) {
        Serial.println("CAN initialization failed!");
        Serial.println("Check MCP2515 wiring and power");
    }

    // Start in sniff mode
    canSetMode(CAN_MODE_SNIFF);
    canSetRpmMessageId(RPM_CAN_ID);
    canSetRpmExtraction(RPM_BYTE_OFFSET, RPM_SCALE);

    printHelp();
}

void loop() {
    // Process serial commands
    processSerialCommand();

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
            unsigned long now = millis();
            if (now - lastSendTime >= I2C_SEND_INTERVAL_MS) {
                lastSendTime = now;
                i2cSendRpm(currentRpm);
            }
            break;
        }

        case MODE_SIMULATE: {
            // Generate simulated RPM and send over I2C
            currentRpm = generateSimulatedRpm();
            unsigned long now = millis();
            if (now - lastSendTime >= I2C_SEND_INTERVAL_MS) {
                lastSendTime = now;
                i2cSendRpm(currentRpm);
            }
            break;
        }
    }
}
