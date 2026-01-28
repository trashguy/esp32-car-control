#include "i2c_slave.h"
#include "shared/config.h"
#include "shared/protocol.h"
#include <Arduino.h>
#include <Wire.h>

static RpmCallback rpmCallback = nullptr;
static volatile uint16_t lastRpm = 0;
static volatile unsigned long lastPacketTime = 0;
static volatile uint32_t validPacketCount = 0;
static volatile uint32_t invalidPacketCount = 0;

static uint8_t receiveBuffer[RPM_PACKET_SIZE];
static volatile uint8_t bufferIndex = 0;
static bool isSlaveMode = false;

// I2C receive callback
static void onReceive(int numBytes) {
    // Read all available bytes
    while (Wire.available() && bufferIndex < RPM_PACKET_SIZE) {
        receiveBuffer[bufferIndex++] = Wire.read();
    }

    // Drain any extra bytes
    while (Wire.available()) {
        Wire.read();
    }

    // Process complete packet
    if (bufferIndex == RPM_PACKET_SIZE) {
        if (validatePacket(receiveBuffer, RPM_PACKET_SIZE)) {
            lastRpm = extractRpm(receiveBuffer);
            lastPacketTime = millis();
            validPacketCount++;

            if (rpmCallback != nullptr) {
                rpmCallback(lastRpm);
            }
        } else {
            invalidPacketCount++;
        }
        bufferIndex = 0;
    }
}

bool i2cSlaveInit(RpmCallback callback) {
    rpmCallback = callback;
    // Don't initialize here - display will manage the bus
    Serial.printf("I2C Slave registered at address 0x%02X (bus managed by display)\n",
                  I2C_SLAVE_ADDRESS);
    return true;
}

void i2cEnableSlaveMode() {
    if (!isSlaveMode) {
        Wire.end();
        Wire.begin(I2C_SLAVE_ADDRESS, I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQUENCY);
        Wire.onReceive(onReceive);
        isSlaveMode = true;
    }
}

void i2cEnableMasterMode() {
    if (isSlaveMode) {
        Wire.end();
        Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
        Wire.setClock(I2C_FREQUENCY);  // Fast mode
        isSlaveMode = false;
    }
}

uint16_t i2cGetLastRpm() {
    return lastRpm;
}

unsigned long i2cGetTimeSinceLastPacket() {
    if (lastPacketTime == 0) {
        return ULONG_MAX;
    }
    return millis() - lastPacketTime;
}

bool i2cIsConnected() {
    return i2cGetTimeSinceLastPacket() < I2C_TIMEOUT_MS;
}

uint32_t i2cGetValidPacketCount() {
    return validPacketCount;
}

uint32_t i2cGetInvalidPacketCount() {
    return invalidPacketCount;
}
