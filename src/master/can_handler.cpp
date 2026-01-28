#include "can_handler.h"
#include "shared/config.h"
#include <Arduino.h>
#include <SPI.h>
#include <mcp2515.h>

// CAN Configuration - adjust based on your MCP2515 module and vehicle
static const CAN_SPEED CAN_BITRATE = CAN_500KBPS;  // Volvo typically uses 500kbps
static const CAN_CLOCK CAN_CLOCK_SPEED = MCP_8MHZ; // Crystal on MCP2515 module

static SPIClass* canSpi = nullptr;
static MCP2515* canController = nullptr;
static bool canInitialized = false;

static CanMode currentMode = CAN_MODE_SNIFF;
static uint32_t rpmMessageId = 0;
static uint8_t rpmByteOffset = 0;
static float rpmScale = 1.0f;

static uint32_t messageCount = 0;
static uint32_t errorCount = 0;

bool canInit() {
    canInitialized = false;

    // Initialize SPI for MCP2515 using FSPI (SPI2) - separate from comm SPI on HSPI
    canSpi = new SPIClass(FSPI);
    if (!canSpi) {
        Serial.println("Failed to allocate SPI!");
        return false;
    }
    canSpi->begin(MCP2515_SCK_PIN, MCP2515_MISO_PIN, MCP2515_MOSI_PIN, MCP2515_CS_PIN);

    // Initialize MCP2515 (CS pin, SPI clock speed, SPI instance)
    canController = new MCP2515(MCP2515_CS_PIN, 10000000, canSpi);
    if (!canController) {
        Serial.println("Failed to allocate MCP2515!");
        return false;
    }

    MCP2515::ERROR result = canController->reset();
    if (result != MCP2515::ERROR_OK) {
        Serial.println("MCP2515 reset failed - CAN disabled");
        errorCount++;
        return false;
    }

    result = canController->setBitrate(CAN_BITRATE, CAN_CLOCK_SPEED);
    if (result != MCP2515::ERROR_OK) {
        Serial.println("MCP2515 setBitrate failed - CAN disabled");
        errorCount++;
        return false;
    }

    result = canController->setNormalMode();
    if (result != MCP2515::ERROR_OK) {
        Serial.println("MCP2515 setNormalMode failed - CAN disabled");
        errorCount++;
        return false;
    }

    canInitialized = true;
    Serial.println("MCP2515 initialized successfully");
    return true;
}

void canSetMode(CanMode mode) {
    currentMode = mode;
    if (mode == CAN_MODE_SNIFF) {
        Serial.println("CAN mode: SNIFF - logging all messages");
    } else {
        Serial.printf("CAN mode: RPM - filtering for ID 0x%03X\n", rpmMessageId);
    }
}

void canSetRpmMessageId(uint32_t messageId) {
    rpmMessageId = messageId;
}

void canSetRpmExtraction(uint8_t byteOffset, float scale) {
    rpmByteOffset = byteOffset;
    rpmScale = scale;
}

static void printCanMessage(const struct can_frame& frame) {
    // Print timestamp
    Serial.printf("[%lu] ", millis());

    // Print ID (standard or extended)
    if (frame.can_id & CAN_EFF_FLAG) {
        Serial.printf("EXT ID: 0x%08X ", frame.can_id & CAN_EFF_MASK);
    } else {
        Serial.printf("STD ID: 0x%03X ", frame.can_id & CAN_SFF_MASK);
    }

    // Print RTR flag
    if (frame.can_id & CAN_RTR_FLAG) {
        Serial.print("RTR ");
    }

    // Print data length
    Serial.printf("DLC: %d ", frame.can_dlc);

    // Print data bytes
    Serial.print("Data: ");
    for (int i = 0; i < frame.can_dlc; i++) {
        Serial.printf("%02X ", frame.data[i]);
    }

    // Print ASCII representation
    Serial.print(" | ");
    for (int i = 0; i < frame.can_dlc; i++) {
        char c = frame.data[i];
        Serial.print((c >= 32 && c <= 126) ? c : '.');
    }

    Serial.println();
}

static uint16_t extractRpmFromFrame(const struct can_frame& frame) {
    if (rpmByteOffset + 1 >= frame.can_dlc) {
        return 0;
    }

    // Extract 16-bit value (little-endian)
    uint16_t rawValue = frame.data[rpmByteOffset] | (frame.data[rpmByteOffset + 1] << 8);
    return (uint16_t)(rawValue * rpmScale);
}

bool canProcess(uint16_t* rpm) {
    if (!canInitialized || canController == nullptr) {
        return false;
    }

    struct can_frame frame;
    MCP2515::ERROR result = canController->readMessage(&frame);

    if (result != MCP2515::ERROR_OK) {
        if (result != MCP2515::ERROR_NOMSG) {
            errorCount++;
        }
        return false;
    }

    messageCount++;

    if (currentMode == CAN_MODE_SNIFF) {
        printCanMessage(frame);
        return false;
    }

    // RPM mode - check if this is our target message
    uint32_t msgId = frame.can_id & (frame.can_id & CAN_EFF_FLAG ? CAN_EFF_MASK : CAN_SFF_MASK);
    if (msgId == rpmMessageId && rpm != nullptr) {
        *rpm = extractRpmFromFrame(frame);
        return true;
    }

    return false;
}

uint32_t canGetMessageCount() {
    return messageCount;
}

uint32_t canGetErrorCount() {
    return errorCount;
}
