#ifndef MASTER_OTA_HANDLER_H
#define MASTER_OTA_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

// =============================================================================
// Master OTA Handler
// =============================================================================
//
// Handles receiving firmware updates from the slave (display MCU) over SPI.
// The slave receives OTA packages via WiFi and stores controller.bin on SD.
// The master polls for updates and downloads firmware when available.
//
// =============================================================================

// OTA polling interval during normal operation
#define OTA_POLL_INTERVAL_MS 5000  // Check every 5 seconds

// Fast polling interval when in OTA mode (waiting for user input)
#define OTA_POLL_FAST_MS 200  // Check every 200ms when in OTA mode

// Chunk transfer timeout
#define OTA_CHUNK_TIMEOUT_MS 1000  // 1 second per chunk

// Maximum retries per chunk
#define OTA_CHUNK_MAX_RETRIES 3

// =============================================================================
// OTA State Machine
// =============================================================================

enum MasterOtaState {
    MASTER_OTA_IDLE,            // Normal operation, periodic polling
    MASTER_OTA_POLLING,         // In OTA mode, fast polling for status
    MASTER_OTA_WAITING,         // Verification passed, waiting for user to press INSTALL
    MASTER_OTA_DOWNLOADING,     // Downloading firmware from slave
    MASTER_OTA_VERIFYING,       // Verifying downloaded firmware
    MASTER_OTA_FLASHING,        // Flashing firmware
    MASTER_OTA_COMPLETE,        // Update complete, pending reboot
    MASTER_OTA_ERROR            // Error occurred
};

// =============================================================================
// API Functions
// =============================================================================

// Initialize OTA handler (call during setup)
void masterOtaInit();

// Process OTA state machine (call from SPI task, non-blocking)
// Returns true if OTA is in progress (skip normal SPI exchange)
bool masterOtaProcess();

// Get current OTA state
MasterOtaState masterOtaGetState();

// Get OTA progress (0-100)
uint8_t masterOtaGetProgress();

// Get error message (valid when state is ERROR)
const char* masterOtaGetErrorMessage();

// Check if reboot is pending after successful update
bool masterOtaRebootPending();

// Perform reboot after successful update
void masterOtaReboot();

// =============================================================================
// OTA Test Mode
// =============================================================================

// Run OTA protocol test (exercises handshake, bulk mode, data transfer)
// Returns true if test passed, false on failure
// This is a blocking call that takes ~2-3 seconds
bool masterOtaRunTest();

#endif // MASTER_OTA_HANDLER_H
