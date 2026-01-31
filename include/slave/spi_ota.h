#ifndef SLAVE_SPI_OTA_H
#define SLAVE_SPI_OTA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// =============================================================================
// Slave SPI OTA Handler
// =============================================================================
//
// Handles serving controller firmware to the master over SPI.
// The slave receives OTA packages via WiFi and stores controller.bin on SD.
// When the master polls with OTA commands, this handler responds.
//
// =============================================================================

// Check if there's controller firmware available for the master
bool spiOtaHasFirmware();

// Get controller firmware size
uint32_t spiOtaGetFirmwareSize();

// Get controller firmware CRC32
uint32_t spiOtaGetFirmwareCrc();

// Read a chunk of firmware data
// Returns bytes read, or 0 on error
size_t spiOtaReadChunk(uint16_t chunkIndex, uint8_t* buffer, size_t maxLen);

// Mark firmware as transferred (cleanup)
void spiOtaClearFirmware();

// =============================================================================
// Verification State Management
// =============================================================================

// Request SPI verification test (called when user presses VERIFY)
void spiOtaRequestVerify();

// Check if verification is requested (master polls this)
bool spiOtaIsVerifyRequested();

// Set verification result (called when master reports result)
void spiOtaSetVerifyResult(bool passed);

// Get verification state: 0=none, 1=requested, 2=passed, 3=failed
uint8_t spiOtaGetVerifyState();

// Clear verification state (after ABORT or new popup)
void spiOtaClearVerifyState();

// =============================================================================
// OTA Mode Control
// =============================================================================

// Check if OTA mode is active (slave should ignore normal SPI packets)
bool spiOtaIsActive();

// Enter OTA mode (called when VERIFY is pressed)
void spiOtaEnterMode();

// Exit OTA mode (called on ABORT or completion)
void spiOtaExitMode();

// =============================================================================
// Packet Processing
// =============================================================================
// Returns true if packet was an OTA packet and was handled
// txResponse buffer should be filled with response data
// enterBulkMode is set to true when master requests bulk transfer mode
// exitBulkMode is set to true when OTA is complete (DONE/ABORT received)
bool spiOtaProcessPacket(const uint8_t* rxData, size_t rxLen, 
                          uint8_t* txResponse, size_t* txLen,
                          bool* enterBulkMode, bool* exitBulkMode);

#endif // SLAVE_SPI_OTA_H
