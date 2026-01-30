#ifndef SCREEN_MAIN_H
#define SCREEN_MAIN_H

#include "display_common.h"

// =============================================================================
// Main Screen Layout Constants
// =============================================================================

#define RPM_Y_POS     80
#define LABEL_Y_POS   140

// Sync status dot (top left)
#define SYNC_DOT_X    12
#define SYNC_DOT_Y    12
#define SYNC_DOT_R    5

// Logo layout
#define LOGO_Y_POS    (SCREEN_HEIGHT - 30)

// Gear button (bottom right)
#define GEAR_BTN_SIZE   36
#define GEAR_BTN_X      (SCREEN_WIDTH - GEAR_BTN_SIZE - 8)
#define GEAR_BTN_Y      (SCREEN_HEIGHT - GEAR_BTN_SIZE - 8)

// Mode button (bottom left)
#define MODE_BTN_SIZE   36
#define MODE_BTN_X      8
#define MODE_BTN_Y      (SCREEN_HEIGHT - MODE_BTN_SIZE - 8)

// RPM up/down buttons (manual mode only)
#define RPM_BTN_SIZE    40
#define RPM_BTN_Y       (RPM_Y_POS - RPM_BTN_SIZE / 2)
#define RPM_UP_BTN_X    20
#define RPM_DOWN_BTN_X  (SCREEN_WIDTH - RPM_BTN_SIZE - 20)

// =============================================================================
// Main Screen Functions
// =============================================================================

// Initialize main screen state
void screenMainInit();

// Draw the complete main screen
void screenMainDraw();

// Handle touch events
void screenMainHandleTouch(int16_t x, int16_t y, bool pressed);

// Update animations (blink, etc)
void screenMainUpdate();

// Update RPM display
void screenMainUpdateRpm(uint16_t rpm, bool connected);

// Update connection status
void screenMainSetConnected(bool connected);

#endif // SCREEN_MAIN_H
