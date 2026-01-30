#ifndef ENCODER_MUX_H
#define ENCODER_MUX_H

#include <stdint.h>

// =============================================================================
// MCP23017-Based Rotary Encoder Multiplexer
// =============================================================================
// Provides support for up to 5 rotary encoders via I2C GPIO expander.
// Uses internal pull-ups on MCP23017 - no external resistors needed.
//
// Hardware requirements:
//   - MCP23017 I2C GPIO expander at address 0x20
//   - I2C bus on GPIO 47 (SDA) and GPIO 48 (SCL)
//   - Optional: Interrupt on GPIO 45 for responsive detection
//
// Encoder assignments:
//   - ENC1 (J14): Power Steering Speed Control
//   - ENC2-ENC5 (J15-J18): Available for future use
// =============================================================================

// Maximum number of encoders supported
#define ENCODER_MUX_MAX_ENCODERS 5

// Encoder indices
#define ENCODER_POWER_STEERING  0
#define ENCODER_2               1
#define ENCODER_3               2
#define ENCODER_4               3
#define ENCODER_5               4

// Encoder event types
typedef enum {
    ENCODER_EVENT_NONE = 0,
    ENCODER_EVENT_CW,           // Clockwise rotation
    ENCODER_EVENT_CCW,          // Counter-clockwise rotation
    ENCODER_EVENT_BUTTON_DOWN,  // Button pressed
    ENCODER_EVENT_BUTTON_UP     // Button released
} EncoderEvent;

// Encoder state structure
typedef struct {
    int32_t position;           // Current position (accumulated)
    int32_t minValue;           // Minimum allowed value
    int32_t maxValue;           // Maximum allowed value
    int32_t stepSize;           // Step size per detent
    bool buttonState;           // Current button state (true = pressed)
    bool buttonChanged;         // Button state changed since last read
    bool enabled;               // Encoder is enabled and connected
} EncoderState;

// Callback function type for encoder events
typedef void (*EncoderCallback)(uint8_t encoderIndex, EncoderEvent event, int32_t position);

// =============================================================================
// Initialization
// =============================================================================

// Initialize the encoder multiplexer
// Returns true on success, false if MCP23017 not found
bool encoderMuxInit();

// Enable/disable the encoder multiplexer
void encoderMuxEnable();
void encoderMuxDisable();

// Check if encoder multiplexer is enabled
bool encoderMuxIsEnabled();

// =============================================================================
// Encoder Configuration
// =============================================================================

// Configure an encoder's range and step size
// encoderIndex: 0-4 (ENCODER_POWER_STEERING, etc.)
// minValue: Minimum position value
// maxValue: Maximum position value
// stepSize: Position change per detent
// initialValue: Starting position
void encoderMuxConfigure(uint8_t encoderIndex, int32_t minValue, int32_t maxValue,
                         int32_t stepSize, int32_t initialValue);

// Enable/disable specific encoder
void encoderMuxSetEnabled(uint8_t encoderIndex, bool enabled);

// Set callback for encoder events (optional)
void encoderMuxSetCallback(EncoderCallback callback);

// =============================================================================
// Reading Encoder State
// =============================================================================

// Update all encoder states (call in loop or timer)
// Returns true if any encoder changed
bool encoderMuxUpdate();

// Get current position of an encoder
int32_t encoderMuxGetPosition(uint8_t encoderIndex);

// Set position of an encoder
void encoderMuxSetPosition(uint8_t encoderIndex, int32_t position);

// Get button state (true = pressed)
bool encoderMuxGetButton(uint8_t encoderIndex);

// Check if button was pressed since last check (auto-clears)
bool encoderMuxButtonPressed(uint8_t encoderIndex);

// Get full encoder state
EncoderState* encoderMuxGetState(uint8_t encoderIndex);

// =============================================================================
// Power Steering Convenience Functions
// =============================================================================

// Get power steering assist level (0-100%)
uint8_t encoderMuxGetPowerSteeringLevel();

// Set power steering assist level (0-100%)
void encoderMuxSetPowerSteeringLevel(uint8_t level);

// =============================================================================
// Diagnostics
// =============================================================================

// Check if MCP23017 is responding
bool encoderMuxIsConnected();

// Get raw GPIO state from MCP23017 (for debugging)
uint16_t encoderMuxGetRawGPIO();

// Get number of encoder updates processed
uint32_t encoderMuxGetUpdateCount();

#endif // ENCODER_MUX_H
