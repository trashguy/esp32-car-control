#include "i2c_master.h"
#include "shared/config.h"
#include "shared/protocol.h"
#include <Arduino.h>
#include <Wire.h>

static uint32_t successCount = 0;
static uint32_t errorCount = 0;

bool i2cMasterInit() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQUENCY);
    Serial.printf("I2C Master initialized on SDA=%d, SCL=%d\n", I2C_SDA_PIN, I2C_SCL_PIN);
    return true;
}

bool i2cSendRpm(uint16_t rpm) {
    uint8_t buffer[RPM_PACKET_SIZE];
    packRpmPacket(buffer, rpm);

    Wire.beginTransmission(I2C_SLAVE_ADDRESS);
    size_t written = Wire.write(buffer, RPM_PACKET_SIZE);
    uint8_t result = Wire.endTransmission();

    if (result == 0 && written == RPM_PACKET_SIZE) {
        successCount++;
        return true;
    }

    errorCount++;
    if (result != 0) {
        Serial.printf("I2C transmission error: %d\n", result);
    }
    return false;
}

uint32_t i2cGetSuccessCount() {
    return successCount;
}

uint32_t i2cGetErrorCount() {
    return errorCount;
}
