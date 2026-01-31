#ifndef SPI_SLAVE_H
#define SPI_SLAVE_H

#include <stdint.h>

// Callback type for when data is received from master
// Master sends: RPM to display, authoritative mode
typedef void (*MasterDataCallback)(uint16_t rpm, uint8_t mode);

// Initialize SPI slave for communication with master device
bool spiSlaveInit(MasterDataCallback callback);

// Update the data that will be sent to master on next exchange
// This sends UI input requests to master (slave requests, master decides)
void spiSlaveSetRequest(uint8_t requestedMode, uint16_t requestedRpm);

// Process any pending SPI transactions (call from loop)
void spiSlaveProcess();

// Get the last received RPM from master (authoritative)
uint16_t spiSlaveGetLastRpm();

// Get the last received mode from master (authoritative)
uint8_t spiSlaveGetMasterMode();

// Get water temperature from master (Fahrenheit * 10)
// Returns WATER_TEMP_INVALID if not received or sensor error
int16_t spiSlaveGetWaterTempF10();

// Get water temperature status from master
// Returns WATER_TEMP_STATUS_* value from protocol.h
uint8_t spiSlaveGetWaterTempStatus();

// Get time since last valid packet (ms)
unsigned long spiSlaveGetTimeSinceLastPacket();

// Check if connection is active
bool spiSlaveIsConnected();

// Get statistics
uint32_t spiSlaveGetValidPacketCount();
uint32_t spiSlaveGetInvalidPacketCount();

// Requested state (what slave UI wants - sent to master)
void spiSlaveSetRequestedMode(uint8_t mode);
uint8_t spiSlaveGetRequestedMode();
void spiSlaveSetRequestedRpm(uint16_t rpm);
uint16_t spiSlaveGetRequestedRpm();

// Check if we just reconnected (returns true once, then resets)
// Use this to force display refresh on reconnection
bool spiSlaveCheckReconnected();

#endif // SPI_SLAVE_H
