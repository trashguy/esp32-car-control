#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include "display_common.h"

// Initialize display and touch
bool displayInit();

// Update display with new RPM value
void displayUpdateRpm(uint16_t rpm);

// Set connection state (affects status indicator)
void displaySetConnected(bool connected);

// Get touch input (returns true if touched, fills x/y)
bool displayGetTouch(int16_t* x, int16_t* y);

// Process display updates (call from loop or display task)
void displayLoop();

// Get current screen
ScreenType displayGetScreen();

// =============================================================================
// Thread-Safe UI Command Sending (for FreeRTOS mode)
// =============================================================================

// Send mode change request to SPI task via queue
// Returns true if queued successfully
bool displaySendModeRequest(uint8_t mode);

// Send RPM change request to SPI task via queue
// Returns true if queued successfully
bool displaySendRpmRequest(uint16_t rpm);

// Send combined mode+RPM request to SPI task
bool displaySendRequest(uint8_t mode, uint16_t rpm);

#endif // DISPLAY_H
