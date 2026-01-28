#ifndef I2C_SLAVE_H
#define I2C_SLAVE_H

#include <stdint.h>

// Callback type for when new RPM is received
typedef void (*RpmCallback)(uint16_t rpm);

// Initialize I2C as slave with callback for RPM updates
bool i2cSlaveInit(RpmCallback callback);

// Get last received RPM value
uint16_t i2cGetLastRpm();

// Get time since last valid packet (milliseconds)
unsigned long i2cGetTimeSinceLastPacket();

// Check if communication is active (received packet within timeout)
bool i2cIsConnected();

// Get reception statistics
uint32_t i2cGetValidPacketCount();
uint32_t i2cGetInvalidPacketCount();

// I2C bus mode switching (for shared bus with touch)
void i2cEnableSlaveMode();
void i2cEnableMasterMode();

#endif // I2C_SLAVE_H
