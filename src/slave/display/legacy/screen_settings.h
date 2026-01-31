#ifndef SCREEN_SETTINGS_H
#define SCREEN_SETTINGS_H

#include "../display_common.h"

// =============================================================================
// Settings Screen Layout Constants
// =============================================================================

// Content area
#define DIAG_CONTENT_Y      40
#define DIAG_CONTENT_H      (SCREEN_HEIGHT - DIAG_CONTENT_Y - 50)

// Back button (center bottom)
#define BACK_BTN_WIDTH  80
#define BACK_BTN_HEIGHT 36
#define BACK_BTN_X      ((SCREEN_WIDTH - BACK_BTN_WIDTH) / 2)
#define BACK_BTN_Y      (SCREEN_HEIGHT - BACK_BTN_HEIGHT - 10)

// SD card button (bottom right)
#define SD_BTN_SIZE     36
#define SD_BTN_X        (SCREEN_WIDTH - SD_BTN_SIZE - 8)
#define SD_BTN_Y        (SCREEN_HEIGHT - SD_BTN_SIZE - 8)

// USB MSC toggle button (next to SD button)
#define USB_BTN_SIZE    36
#define USB_BTN_X       (SD_BTN_X - USB_BTN_SIZE - 8)
#define USB_BTN_Y       (SCREEN_HEIGHT - USB_BTN_SIZE - 8)

// WiFi button (bottom left)
#define SETTINGS_WIFI_BTN_SIZE  36
#define SETTINGS_WIFI_BTN_X     8
#define SETTINGS_WIFI_BTN_Y     (SCREEN_HEIGHT - SETTINGS_WIFI_BTN_SIZE - 8)

// =============================================================================
// Settings Screen Functions
// =============================================================================

// Draw the complete settings screen
void screenSettingsDraw();

// Handle touch events
void screenSettingsHandleTouch(int16_t x, int16_t y, bool pressed);

// Update (for scroll animations, etc)
void screenSettingsUpdate();

// Reset scroll position
void screenSettingsReset();

#endif // SCREEN_SETTINGS_H
