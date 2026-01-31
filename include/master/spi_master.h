#ifndef SPI_MASTER_H
#define SPI_MASTER_H

#include <stdint.h>
#include <stddef.h>

// Initialize SPI master for communication with slave device
bool spiMasterInit();

// Send RPM and mode to slave, receive requested mode/RPM back (full-duplex)
// Master is authoritative - sends state, receives UI input requests
// Returns true if valid response received from slave
bool spiExchange(uint16_t rpmToSend, uint8_t modeToSend, uint8_t* requestedMode, uint16_t* requestedRpm);

// =============================================================================
// OTA SPI Functions
// =============================================================================

// Standard OTA packet exchange (5 bytes)
bool spiOtaExchange(const uint8_t* txBuffer, uint8_t* rxBuffer, size_t len);

// Bulk data exchange for OTA (larger packets)
bool spiOtaExchangeBulk(const uint8_t* txBuffer, uint8_t* rxBuffer, size_t len);

// Get statistics
uint32_t spiGetSuccessCount();
uint32_t spiGetErrorCount();

#endif // SPI_MASTER_H
