#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

// Display states
enum DisplayState {
    DISPLAY_NO_SIGNAL,
    DISPLAY_CONNECTED
};

// Screen types
enum ScreenType {
    SCREEN_MAIN,
    SCREEN_DIAGNOSTICS,
    SCREEN_FILE_BROWSER,
    SCREEN_WIFI
};

// Initialize display and touch
bool displayInit();

// Update display with new RPM value
void displayUpdateRpm(uint16_t rpm);

// Set connection state (affects status indicator)
void displaySetConnected(bool connected);

// Get touch input (returns true if touched, fills x/y)
bool displayGetTouch(int16_t* x, int16_t* y);

// Process display updates (call from loop)
void displayLoop();

// Get current screen
ScreenType displayGetScreen();

#endif // DISPLAY_H
