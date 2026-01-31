#ifndef SCREEN_OTA_POPUP_H
#define SCREEN_OTA_POPUP_H

#include "../display_common.h"
#include <stdint.h>
#include <stdbool.h>

// =============================================================================
// OTA Popup Layout Constants
// =============================================================================

#define OTA_POPUP_W         280
#define OTA_POPUP_HEIGHT    140
#define OTA_POPUP_X         ((SCREEN_WIDTH - OTA_POPUP_W) / 2)
#define OTA_POPUP_Y         ((SCREEN_HEIGHT - OTA_POPUP_HEIGHT) / 2)

#define OTA_POPUP_BTN_W     100
#define OTA_POPUP_BTN_H     35
#define OTA_POPUP_BTN_Y     (OTA_POPUP_Y + OTA_POPUP_HEIGHT - OTA_POPUP_BTN_H - 15)

#define OTA_POPUP_INSTALL_X (OTA_POPUP_X + 25)
#define OTA_POPUP_LATER_X   (OTA_POPUP_X + OTA_POPUP_W - OTA_POPUP_BTN_W - 25)

// =============================================================================
// OTA Popup States
// =============================================================================

enum OtaPopupState {
    OTA_POPUP_HIDDEN,           // Popup not visible
    OTA_POPUP_CONFIRM,          // Showing VERIFY/ABORT buttons (initial state)
    OTA_POPUP_VERIFYING,        // Running SPI verification test
    OTA_POPUP_VERIFIED,         // Test passed, showing INSTALL/ABORT buttons
    OTA_POPUP_INSTALLING,       // Showing progress bar
    OTA_POPUP_CONTROLLER,       // Installing controller firmware
    OTA_POPUP_COMPLETE,         // Update complete, show OK
    OTA_POPUP_ERROR             // Error occurred, show DISMISS
};

// =============================================================================
// OTA Popup Functions
// =============================================================================

// Show the update popup (call when package is ready)
void otaPopupShow();

// Hide the popup
void otaPopupHide();

// Check if popup is currently visible
bool otaPopupIsVisible();

// Get current popup state
OtaPopupState otaPopupGetState();

// Draw the popup (call from screen draw function)
void otaPopupDraw();

// Handle touch events (call from screen touch handler)
// Returns true if touch was consumed by popup
bool otaPopupHandleTouch(int16_t x, int16_t y, bool pressed);

// Update popup state (call from screen update function)
void otaPopupUpdate();

// Set progress for installing state (0-100)
void otaPopupSetProgress(uint8_t progress);

// Set error message and switch to error state
void otaPopupSetError(const char* message);

// Set complete state
void otaPopupSetComplete();

#endif // SCREEN_OTA_POPUP_H
