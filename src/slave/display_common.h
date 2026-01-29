#ifndef DISPLAY_COMMON_H
#define DISPLAY_COMMON_H

#include <stdint.h>
#include <TFT_eSPI.h>

// =============================================================================
// Screen Types & States
// =============================================================================

enum ScreenType {
    SCREEN_MAIN,
    SCREEN_DIAGNOSTICS,
    SCREEN_FILE_BROWSER,
    SCREEN_WIFI
};

enum DisplayState {
    DISPLAY_NO_SIGNAL,
    DISPLAY_CONNECTED
};

// =============================================================================
// Color Scheme (Dark Mode)
// =============================================================================

#define COLOR_BACKGROUND   0x0000    // Pure black
#define COLOR_RPM_TEXT     0xFFFF    // Pure white
#define COLOR_LABEL        0xFFFF    // Pure white
#define COLOR_CONNECTED    0x2DC9    // Muted green (#2D9D4A)
#define COLOR_DISCONNECTED 0xD8A3    // Muted red (#DC3545)
#define COLOR_WARNING      0xFD20    // Muted amber (#FFC107)
#define COLOR_BTN_NORMAL   0x2945    // Dark blue (#293A4A)
#define COLOR_BTN_PRESSED  0x3B8F    // Lighter blue (#3D5A73)
#define COLOR_BTN_TEXT     0xDEFB    // Soft white (#DEE2E6)
#define COLOR_KB_BG        0x1082    // Keyboard background

// =============================================================================
// Screen Layout Constants
// =============================================================================

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// Button styling
#define BTN_RADIUS    8

// WiFi status indicator (top right of all screens)
#define WIFI_STATUS_X    (SCREEN_WIDTH - 25)
#define WIFI_STATUS_Y    12
#define WIFI_STATUS_SIZE 16

// =============================================================================
// Touch State
// =============================================================================

struct TouchState {
    bool lastTouchState;
    int16_t lastTouchY;
    bool isDragging;
};

// =============================================================================
// Global TFT Instance Access
// =============================================================================

// Get the shared TFT instance
TFT_eSPI& getTft();

// =============================================================================
// Common Drawing Utilities
// =============================================================================

// Draw a rounded button with text
void drawButton(int16_t x, int16_t y, int16_t w, int16_t h,
                const char* label, bool pressed, uint16_t textColor = COLOR_BTN_TEXT);

// Draw a rounded button with custom colors
void drawButtonColored(int16_t x, int16_t y, int16_t w, int16_t h,
                       const char* label, uint16_t bgColor, uint16_t textColor);

// Icon drawing functions
void drawGearIcon(int16_t cx, int16_t cy, int16_t size, uint16_t color);
void drawSdCardIcon(int16_t cx, int16_t cy, int16_t size, uint16_t color);
void drawBackArrowIcon(int16_t cx, int16_t cy, int16_t size, uint16_t color);
void drawWifiIcon(int16_t cx, int16_t cy, int16_t size, uint16_t color);

// WiFi status indicator (call from any screen)
void drawWifiStatusIndicator();

// =============================================================================
// Touch Handling
// =============================================================================

// Initialize touch controller
bool touchInit();

// Get touch input (returns true if touched, fills x/y)
bool touchGetPoint(int16_t* x, int16_t* y);

// Check if point is within rectangle
inline bool pointInRect(int16_t px, int16_t py,
                        int16_t rx, int16_t ry, int16_t rw, int16_t rh) {
    return (px >= rx && px <= rx + rw && py >= ry && py <= ry + rh);
}

// =============================================================================
// Screen Interface
// =============================================================================

// Each screen module implements these
struct ScreenInterface {
    void (*draw)();                                    // Full screen redraw
    void (*handleTouch)(int16_t x, int16_t y, bool pressed);  // Touch event
    void (*update)();                                  // Periodic update (animations, etc)
};

// Screen switching (implemented in display.cpp)
void switchToScreen(ScreenType screen);

// Get current screen type
ScreenType getCurrentScreen();

// =============================================================================
// WiFi State Access (for status indicator)
// =============================================================================

int getWifiMode();
void setWifiMode(int mode);
bool isWifiConnected();

#endif // DISPLAY_COMMON_H
