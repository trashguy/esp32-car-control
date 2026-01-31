#ifndef OTA_HANDLER_H
#define OTA_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

// =============================================================================
// OTA Configuration
// =============================================================================

// Device identity on network
#define OTA_HOSTNAME "VONDERWAGENCC1"
#define OTA_SERVICE_TYPE "_esp32ota"

// OTA ports
#define OTA_PORT_ARDUINO 3232       // Standard ArduinoOTA port
#define OTA_PORT_PACKAGE 3233       // Custom port for update packages

// OTA password (only used in production builds)
// IMPORTANT: Change this before deploying to production!
#define OTA_PASSWORD "vonderwagencc1-ota"

// Timeouts
#define OTA_RECEIVE_TIMEOUT_MS 30000  // 30 seconds to receive package

// Binary protocol constants (must match ota-pusher)
#define OTA_MAGIC 0x4F544155            // "OTAU" in little endian
#define OTA_PROTOCOL_VERSION 1

// Binary protocol header structure
struct OtaPacketHeader {
    uint32_t magic;             // OTA_MAGIC
    uint32_t version;           // Protocol version
    uint32_t packageSize;       // Total size of package data
    uint32_t reserved;          // For future use
};

// SD card paths for OTA files
#define OTA_DIR "/ota"
#define OTA_PACKAGE_PATH "/ota/update.zip"
#define OTA_MANIFEST_PATH "/ota/manifest.json"
#define OTA_DISPLAY_FW_PATH "/ota/display.bin"
#define OTA_CONTROLLER_FW_PATH "/ota/controller.bin"
#define OTA_STATE_PATH "/ota/state.json"

// =============================================================================
// OTA State Machine
// =============================================================================

enum OtaState {
    OTA_STATE_IDLE,                 // No OTA in progress
    OTA_STATE_RECEIVING,            // Receiving package via WiFi
    OTA_STATE_PACKAGE_READY,        // Package received, waiting for user
    OTA_STATE_INSTALLING_DISPLAY,   // Installing display firmware
    OTA_STATE_PENDING_CONTROLLER,   // Display done, controller pending
    OTA_STATE_INSTALLING_CONTROLLER,// Installing controller firmware
    OTA_STATE_COMPLETE,             // Update complete
    OTA_STATE_ERROR                 // Error occurred
};

// =============================================================================
// OTA Package Info
// =============================================================================

struct OtaPackageInfo {
    char version[32];
    uint32_t displaySize;
    uint32_t controllerSize;
    char displayMd5[33];
    char controllerMd5[33];
    bool valid;
};

// =============================================================================
// OTA Handler Functions
// =============================================================================

// Initialize OTA handler (call after WiFi connects)
void otaHandlerInit();

// Process OTA events (call from task loop, non-blocking)
void otaHandlerLoop();

// Check if OTA is currently in progress
bool otaInProgress();

// Get current OTA state
OtaState otaGetState();

// Get package info (valid when state is PACKAGE_READY or later)
const OtaPackageInfo* otaGetPackageInfo();

// Get error message (valid when state is ERROR)
const char* otaGetErrorMessage();

// Get current progress (0-100)
uint8_t otaGetProgress();

// Start the update process (called when user clicks INSTALL)
bool otaStartInstall();

// Dismiss pending update (called when user clicks LATER)
void otaDismissUpdate();

// Check if controller update is pending (after display reboot)
bool otaControllerPending();

// Start controller update via SPI (user pressed Update button)
bool otaStartControllerUpdate();

// Check if controller update is actively in progress (SPI transfer happening)
bool otaControllerUpdateInProgress();

// Abort controller update (master returned to normal mode without DONE)
void otaAbortControllerUpdate();

// Clear all OTA state and files
void otaClearState();

// =============================================================================
// Internal - Self Update Progress Display
// =============================================================================

// Called by ArduinoOTA callbacks to update progress on screen
void otaDrawSelfUpdateProgress(unsigned int progress, unsigned int total);
void otaDrawSelfUpdateStart();
void otaDrawSelfUpdateEnd(bool success);

#endif // OTA_HANDLER_H
