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
static unsigned long lastBusRecovery = 0;

// Mode control (sent to master on request)
static volatile uint8_t currentMode = MODE_MANUAL;  // Default to manual mode

// I2C bus recovery - toggle SCL to release stuck slaves
static void i2cBusRecovery() {
    Wire.end();

    // Toggle SCL to release any stuck device
    pinMode(I2C_SCL_PIN, OUTPUT);
    pinMode(I2C_SDA_PIN, INPUT_PULLUP);

    for (int i = 0; i < 9; i++) {
        digitalWrite(I2C_SCL_PIN, HIGH);
        delayMicroseconds(5);
        digitalWrite(I2C_SCL_PIN, LOW);
        delayMicroseconds(5);
    }
    digitalWrite(I2C_SCL_PIN, HIGH);
    delayMicroseconds(5);

    lastBusRecovery = millis();
}

// I2C request callback - send mode to master
static void onRequest() {
    Wire.write(currentMode);
}

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
        Wire.onRequest(onRequest);
        isSlaveMode = true;
    }
}

void i2cEnableMasterMode() {
    if (isSlaveMode) {
        Wire.end();
        Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
        Wire.setClock(I2C_FREQUENCY);
        Wire.setTimeOut(20);  // Short timeout for touch reads
        isSlaveMode = false;
    }
}

void i2cRecoverBus() {
    // Only recover once per second max
    if (millis() - lastBusRecovery < 1000) return;

    Serial.println("I2C bus recovery...");
    i2cBusRecovery();

    // Re-initialize slave mode immediately
    Wire.begin(I2C_SLAVE_ADDRESS, I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQUENCY);
    Wire.onReceive(onReceive);
    Wire.onRequest(onRequest);
    isSlaveMode = true;
    bufferIndex = 0;  // Reset receive buffer

    Serial.println("I2C slave mode re-enabled");
}

void i2cSetMode(uint8_t mode) {
    currentMode = mode;
}

uint8_t i2cGetMode() {
    return currentMode;
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
