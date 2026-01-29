#ifndef SCREEN_WIFI_H
#define SCREEN_WIFI_H

#include "display_common.h"

// =============================================================================
// WiFi Screen Layout Constants
// =============================================================================

// Content area
#define WIFI_CONTENT_Y      40
#define WIFI_CONTENT_H      (SCREEN_HEIGHT - WIFI_CONTENT_Y - 10)

// Mode toggle button
#define WIFI_MODE_BTN_X     10
#define WIFI_MODE_BTN_Y     10   // Relative to content Y
#define WIFI_MODE_BTN_W     300
#define WIFI_MODE_BTN_H     30

// Input fields
#define WIFI_SSID_Y         50   // Relative to content Y
#define WIFI_PASS_Y         90   // Relative to content Y
#define WIFI_INPUT_X        10
#define WIFI_INPUT_W        300
#define WIFI_INPUT_H        28

// Scan button
#define WIFI_SCAN_BTN_X     10
#define WIFI_SCAN_BTN_Y     118  // Relative to content Y
#define WIFI_SCAN_BTN_W     100
#define WIFI_SCAN_BTN_H     26

// Network list
#define WIFI_LIST_Y         150  // Relative to content Y
#define WIFI_LIST_H         22
#define MAX_WIFI_NETWORKS   5
#define MAX_SSID_LEN        32
#define MAX_PASS_LEN        64

// Header back button
#define WIFI_BACK_BTN_X     5
#define WIFI_BACK_BTN_Y     5
#define WIFI_BACK_BTN_W     50
#define WIFI_BACK_BTN_H     26

// Keyboard layout
#define KB_HEADER_H         45
#define KB_KEY_H            36
#define KB_KEY_W            30
#define KB_WIDE_KEY_W       45
#define KB_SPACE_W          120
#define KB_SPACING          2
#define KB_START_Y          (KB_HEADER_H + 5)

// =============================================================================
// WiFi Screen Functions
// =============================================================================

// Initialize WiFi screen (load settings)
void screenWifiInit();

// Draw the complete WiFi screen
void screenWifiDraw();

// Handle touch events
void screenWifiHandleTouch(int16_t x, int16_t y, bool pressed);

// Update (status checks, etc)
void screenWifiUpdate();

// Reset state
void screenWifiReset();

// Check if keyboard is visible (for blocking other updates)
bool screenWifiKeyboardVisible();

// External access to SSID for settings screen display
extern char wifiSsid[MAX_SSID_LEN + 1];

#endif // SCREEN_WIFI_H
