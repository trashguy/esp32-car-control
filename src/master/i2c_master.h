#ifndef I2C_MASTER_H
#define I2C_MASTER_H

#include <stdint.h>

// Initialize I2C as master
bool i2cMasterInit();

// Send RPM value to slave
// Returns true if transmission successful
bool i2cSendRpm(uint16_t rpm);

// Get transmission statistics
uint32_t i2cGetSuccessCount();
uint32_t i2cGetErrorCount();

// Read mode from slave (returns MODE_AUTO, MODE_MANUAL, or 0xFF on error)
uint8_t i2cReadMode();

#endif // I2C_MASTER_H
