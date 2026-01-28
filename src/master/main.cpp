#include <Arduino.h>
#include <esp_task_wdt.h>
#include <Preferences.h>
#include "can_handler.h"
#include "master/spi_master.h"
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
    OP_MODE_SNIFF,      // Log all CAN messages
    OP_MODE_RPM,        // Extract RPM from CAN
    OP_MODE_SIMULATE    // Generate simulated RPM
};

// State
static uint16_t currentRpm = 0;
static unsigned long lastSendTime = 0;
static OperatingMode currentMode = OP_MODE_SIMULATE;  // Start in simulation mode

// Auto mode simulation - simple range 3500-4500
#define SIM_MIN_RPM 3500
#define SIM_MAX_RPM 4500
static unsigned long lastSimChange = 0;
static bool simGoingUp = true;

// Master is source of truth - these values are persisted in NVS
static Preferences prefs;
static uint8_t displayMode = MODE_AUTO;      // Master's authoritative mode
static uint16_t manualRpm = 3000;            // Master's authoritative manual RPM

// SPI communication state
static uint16_t lastSentRpm = 0;
static uint8_t lastRequestedMode = MODE_AUTO;
static uint16_t lastRequestedRpm = 3000;

// Rotary encoder state
static int lastEncoderClk = HIGH;
static unsigned long lastButtonPress = 0;
#define BUTTON_DEBOUNCE_MS 200

// Save master state to NVS
static void saveSettings() {
    prefs.begin("master", false);
    prefs.putUChar("mode", displayMode);
    prefs.putUShort("manualRpm", manualRpm);
    prefs.end();
}

// Load master state from NVS
static void loadSettings() {
    prefs.begin("master", true);
    displayMode = prefs.getUChar("mode", MODE_AUTO);
    manualRpm = prefs.getUShort("manualRpm", 3000);
    prefs.end();
    Serial.printf("Loaded settings: mode=%s, manualRpm=%u\n",
                  displayMode == MODE_AUTO ? "AUTO" : "MANUAL", manualRpm);
}

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
        case OP_MODE_SNIFF:    return "SNIFF";
        case OP_MODE_RPM:      return "RPM";
        case OP_MODE_SIMULATE: return "SIMULATE";
        default:               return "UNKNOWN";
    }
}

void printHelp() {
    Serial.println("\n=== CAN-to-SPI Master ===");
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
    Serial.printf("SPI Success: %lu\n", spiGetSuccessCount());
    Serial.printf("SPI Errors: %lu\n", spiGetErrorCount());
    Serial.printf("Current RPM: %u\n", currentRpm);
    Serial.printf("Last Sent RPM: %u\n", lastSentRpm);
    Serial.printf("Operating Mode: %s\n", getModeName(currentMode));
    if (currentMode == OP_MODE_SIMULATE) {
        Serial.printf("Display Mode: %s (master authoritative)\n", displayMode == MODE_AUTO ? "AUTO" : "MANUAL");
        Serial.printf("Manual RPM: %u (encoder range: %d-%d)\n", manualRpm, ENCODER_RPM_MIN, ENCODER_RPM_MAX);
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
            currentMode = OP_MODE_SNIFF;
            canSetMode(CAN_MODE_SNIFF);
            Serial.println("Sniff mode enabled");
            break;

        case 'r':
        case 'R':
            currentMode = OP_MODE_RPM;
            canSetMode(CAN_MODE_RPM);
            Serial.println("RPM extraction mode enabled");
            break;

        case 'm':
        case 'M':
            currentMode = OP_MODE_SIMULATE;
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
                uint8_t reqMode;
                uint16_t reqRpm;
                if (spiExchange(testRpm, displayMode, &reqMode, &reqRpm)) {
                    Serial.printf("Sent test RPM: %u, mode: %s\n", testRpm,
                                  displayMode == MODE_AUTO ? "AUTO" : "MANUAL");
                    Serial.printf("Slave requested: mode=%s, rpm=%u\n",
                                  reqMode == MODE_AUTO ? "AUTO" : "MANUAL", reqRpm);
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
    Serial.println("  ESP32-S3 CAN-to-SPI Master");
    Serial.println("========================================\n");

    Serial.printf("CPU Freq: %d MHz\n", getCpuFrequencyMhz());
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());

    // Initialize hardware watchdog timer
    esp_task_wdt_init(WDT_TIMEOUT_SEC, true);  // timeout in seconds, panic on timeout
    esp_task_wdt_add(NULL);  // Add current task to watchdog
    Serial.printf("Watchdog enabled: %d second timeout\n", WDT_TIMEOUT_SEC);

    // Initialize SPI master for slave communication
    if (!spiMasterInit()) {
        Serial.println("SPI initialization failed!");
    }

    // Load master state from NVS (master is source of truth)
    loadSettings();

    // Initialize KY-040 rotary encoder
    pinMode(ENCODER_CLK_PIN, INPUT_PULLUP);
    pinMode(ENCODER_DT_PIN, INPUT_PULLUP);
    pinMode(ENCODER_SW_PIN, INPUT_PULLUP);
    lastEncoderClk = digitalRead(ENCODER_CLK_PIN);
    Serial.printf("Encoder initialized on GPIO %d/%d/%d (CLK/DT/SW)\n",
                  ENCODER_CLK_PIN, ENCODER_DT_PIN, ENCODER_SW_PIN);
    Serial.printf("Button: toggle AUTO/MANUAL, Rotation: adjust RPM (%d-%d, step %d)\n",
                  ENCODER_RPM_MIN, ENCODER_RPM_MAX, ENCODER_RPM_STEP);

    // CAN disabled for debugging
    // if (!canInit()) {
    //     Serial.println("CAN initialization failed!");
    //     Serial.println("Check MCP2515 wiring and power");
    // }

    // Configure CAN (even if starting in sim mode)
    // canSetMode(CAN_MODE_SNIFF);
    // canSetRpmMessageId(RPM_CAN_ID);
    // canSetRpmExtraction(RPM_BYTE_OFFSET, RPM_SCALE);

    // Initialize simulation timer
    lastSimChange = millis();
    currentRpm = SIM_MIN_RPM;
    lastSentRpm = 0;  // Force initial send

    Serial.println("Starting in SIMULATION mode - sending RPM over SPI");
    Serial.printf("Simulation range: %u-%u RPM (alternates every %d ms)\n",
                  SIM_MIN_RPM, SIM_MAX_RPM, SIM_CHANGE_INTERVAL_MS);
    Serial.println("Full-duplex SPI: receiving slave mode and manual RPM");
    printHelp();
}

void loop() {
    // Reset hardware watchdog
    esp_task_wdt_reset();

    // Read rotary encoder
    int currentClk = digitalRead(ENCODER_CLK_PIN);
    if (currentClk != lastEncoderClk && currentClk == LOW) {
        // Rotation detected - check direction
        if (digitalRead(ENCODER_DT_PIN) != currentClk) {
            // Clockwise - increase RPM
            if (manualRpm < ENCODER_RPM_MAX) {
                manualRpm += ENCODER_RPM_STEP;
                saveSettings();
                Serial.printf("Encoder CW: RPM = %u\n", manualRpm);
            }
        } else {
            // Counter-clockwise - decrease RPM
            if (manualRpm >= ENCODER_RPM_STEP) {
                manualRpm -= ENCODER_RPM_STEP;
                saveSettings();
                Serial.printf("Encoder CCW: RPM = %u\n", manualRpm);
            }
        }
    }
    lastEncoderClk = currentClk;

    // Check encoder button (active LOW)
    if (digitalRead(ENCODER_SW_PIN) == LOW) {
        unsigned long now = millis();
        if (now - lastButtonPress > BUTTON_DEBOUNCE_MS) {
            lastButtonPress = now;
            // Toggle mode
            displayMode = (displayMode == MODE_AUTO) ? MODE_MANUAL : MODE_AUTO;
            saveSettings();
            Serial.printf("Encoder button: mode = %s\n",
                          displayMode == MODE_AUTO ? "AUTO" : "MANUAL");
        }
    }

    // Heartbeat debug
    static unsigned long lastHeartbeat = 0;
    if (millis() - lastHeartbeat >= 5000) {
        lastHeartbeat = millis();
        const char* effectiveMode = (currentMode == OP_MODE_SIMULATE && displayMode == MODE_MANUAL)
                                    ? "MANUAL" : getModeName(currentMode);
        Serial.printf("Heartbeat: mode=%s, rpm=%u, heap=%d\n",
                      effectiveMode, currentRpm, ESP.getFreeHeap());
    }

    // Process serial commands
    processSerialCommand();

    unsigned long now = millis();

    // Process based on current mode
    switch (currentMode) {
        case OP_MODE_SNIFF:
            // CAN disabled - do nothing
            // canProcess(nullptr);
            break;

        case OP_MODE_RPM: {
            // CAN disabled - just send current RPM over SPI
            // uint16_t rpm;
            // if (canProcess(&rpm)) {
            //     currentRpm = rpm;
            // }
            now = millis();
            if (now - lastSendTime >= SPI_SEND_INTERVAL_MS) {
                lastSendTime = now;
                uint8_t reqMode;
                uint16_t reqRpm;
                if (spiExchange(currentRpm, displayMode, &reqMode, &reqRpm)) {
                    // Process slave UI requests
                    bool changed = false;
                    if (reqMode != displayMode) {
                        displayMode = reqMode;
                        changed = true;
                    }
                    if (reqRpm != manualRpm) {
                        manualRpm = reqRpm;
                        changed = true;
                    }
                    if (changed) {
                        saveSettings();
                        Serial.printf("Updated from slave request: mode=%s, manualRpm=%u\n",
                                      displayMode == MODE_AUTO ? "AUTO" : "MANUAL", manualRpm);
                    }
                    lastSentRpm = currentRpm;
                }
            }
            break;
        }

        case OP_MODE_SIMULATE: {
            now = millis();

            // Master is source of truth for mode
            // In manual mode, use master's manualRpm
            // In auto mode, generate simulated RPM
            if (displayMode == MODE_MANUAL) {
                currentRpm = manualRpm;
            } else if (now - lastSimChange >= SIM_CHANGE_INTERVAL_MS) {
                // Generate simulated RPM - alternate between min and max
                lastSimChange = now;
                currentRpm = generateNextSimulatedRpm();
            }

            // Send SPI at configured interval
            if (now - lastSendTime >= SPI_SEND_INTERVAL_MS) {
                lastSendTime = now;
                uint8_t reqMode;
                uint16_t reqRpm;
                if (spiExchange(currentRpm, displayMode, &reqMode, &reqRpm)) {
                    // Process slave UI requests - master decides whether to accept
                    bool changed = false;
                    if (reqMode != lastRequestedMode) {
                        lastRequestedMode = reqMode;
                        if (reqMode != displayMode) {
                            displayMode = reqMode;
                            changed = true;
                            Serial.printf("Mode changed to %s (slave request)\n",
                                          displayMode == MODE_AUTO ? "AUTO" : "MANUAL");
                        }
                    }
                    if (reqRpm != lastRequestedRpm) {
                        lastRequestedRpm = reqRpm;
                        if (reqRpm != manualRpm) {
                            manualRpm = reqRpm;
                            changed = true;
                            Serial.printf("Manual RPM changed to %u (slave request)\n", manualRpm);
                        }
                    }
                    if (changed) {
                        saveSettings();
                    }
                    lastSentRpm = currentRpm;
                }
            }
            break;
        }
    }

    // Small delay to prevent tight loop
    delay(1);
}
