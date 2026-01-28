#include "i2c_master.h"
#include "shared/config.h"
#include "shared/protocol.h"
#include <Arduino.h>
#include <Wire.h>

static uint32_t successCount = 0;
static uint32_t errorCount = 0;
static uint32_t consecutiveErrors = 0;

static void i2cBusRecovery() {
    // Try to recover the I2C bus by toggling SCL
    Wire.end();

    // Toggle SCL to release any stuck slave
    pinMode(I2C_SCL_PIN, OUTPUT);
    for (int i = 0; i < 9; i++) {
        digitalWrite(I2C_SCL_PIN, HIGH);
        delayMicroseconds(5);
        digitalWrite(I2C_SCL_PIN, LOW);
        delayMicroseconds(5);
    }
    digitalWrite(I2C_SCL_PIN, HIGH);

    // Reinitialize I2C
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQUENCY);
    Wire.setTimeOut(50);

    Serial.println("I2C bus recovery attempted");
}

bool i2cMasterInit() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQUENCY);
    Wire.setTimeOut(50);  // 50ms timeout to prevent hanging
    Serial.printf("I2C Master initialized on SDA=%d, SCL=%d\n", I2C_SDA_PIN, I2C_SCL_PIN);
    return true;
}

bool i2cSendRpm(uint16_t rpm) {
    uint8_t buffer[RPM_PACKET_SIZE];
    packRpmPacket(buffer, rpm);

    Wire.beginTransmission(I2C_SLAVE_ADDRESS);
    size_t written = Wire.write(buffer, RPM_PACKET_SIZE);
    uint8_t result = Wire.endTransmission(true);  // Send stop

    if (result == 0 && written == RPM_PACKET_SIZE) {
        successCount++;
        consecutiveErrors = 0;
        return true;
    }

    errorCount++;
    consecutiveErrors++;

    // Try bus recovery after 10 consecutive errors
    if (consecutiveErrors >= 10) {
        i2cBusRecovery();
        consecutiveErrors = 0;
    }

    // Don't spam serial with errors - only print occasionally
    if (errorCount % 100 == 1) {
        Serial.printf("I2C errors: %lu (last code: %d)\n", errorCount, result);
    }
    return false;
}

uint32_t i2cGetSuccessCount() {
    return successCount;
}

uint32_t i2cGetErrorCount() {
    return errorCount;
}

// Read mode from slave (returns MODE_AUTO or MODE_MANUAL, or 0xFF on error)
uint8_t i2cReadMode() {
    Wire.requestFrom(I2C_SLAVE_ADDRESS, (uint8_t)1);
    if (Wire.available()) {
        return Wire.read();
    }
    return 0xFF;  // Error
}
