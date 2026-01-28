#ifndef SPI_MASTER_H
#define SPI_MASTER_H

#include <stdint.h>

// Initialize SPI master for communication with slave device
bool spiMasterInit();

// Send RPM and mode to slave, receive requested mode/RPM back (full-duplex)
// Master is authoritative - sends state, receives UI input requests
// Returns true if valid response received from slave
bool spiExchange(uint16_t rpmToSend, uint8_t modeToSend, uint8_t* requestedMode, uint16_t* requestedRpm);

// Get statistics
uint32_t spiGetSuccessCount();
uint32_t spiGetErrorCount();

#endif // SPI_MASTER_H
