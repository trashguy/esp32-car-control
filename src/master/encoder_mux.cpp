#include "encoder_mux.h"
#include "../include/shared/config.h"
#include <Arduino.h>
#include <Wire.h>
#include <esp_log.h>

static const char* TAG = "ENCODER_MUX";

// =============================================================================
// MCP23017 Register Definitions
// =============================================================================

#define MCP23017_IODIRA   0x00  // I/O Direction Register A
#define MCP23017_IODIRB   0x01  // I/O Direction Register B
#define MCP23017_IPOLA    0x02  // Input Polarity Register A
#define MCP23017_IPOLB    0x03  // Input Polarity Register B
#define MCP23017_GPINTENA 0x04  // Interrupt-on-change Enable A
#define MCP23017_GPINTENB 0x05  // Interrupt-on-change Enable B
#define MCP23017_DEFVALA  0x06  // Default Compare Value A
#define MCP23017_DEFVALB  0x07  // Default Compare Value B
#define MCP23017_INTCONA  0x08  // Interrupt Control A
#define MCP23017_INTCONB  0x09  // Interrupt Control B
#define MCP23017_IOCON    0x0A  // Configuration Register
#define MCP23017_GPPUA    0x0C  // Pull-up Resistor A
#define MCP23017_GPPUB    0x0D  // Pull-up Resistor B
#define MCP23017_INTFA    0x0E  // Interrupt Flag A
#define MCP23017_INTFB    0x0F  // Interrupt Flag B
#define MCP23017_INTCAPA  0x10  // Interrupt Capture A
#define MCP23017_INTCAPB  0x11  // Interrupt Capture B
#define MCP23017_GPIOA    0x12  // GPIO Register A
#define MCP23017_GPIOB    0x13  // GPIO Register B
#define MCP23017_OLATA    0x14  // Output Latch A
#define MCP23017_OLATB    0x15  // Output Latch B

// =============================================================================
// Encoder Pin Mapping (MCP23017 pins)
// =============================================================================

typedef struct {
    uint8_t clkPin;   // Channel A pin on MCP23017
    uint8_t dtPin;    // Channel B pin on MCP23017
    uint8_t swPin;    // Switch pin on MCP23017
} EncoderPins;

static const EncoderPins encoderPins[ENCODER_MUX_MAX_ENCODERS] = {
    { ENC1_MCP_CLK, ENC1_MCP_DT, ENC1_MCP_SW },  // J14 - Power Steering
    { ENC2_MCP_CLK, ENC2_MCP_DT, ENC2_MCP_SW },  // J15
    { ENC3_MCP_CLK, ENC3_MCP_DT, ENC3_MCP_SW },  // J16
    { ENC4_MCP_CLK, ENC4_MCP_DT, ENC4_MCP_SW },  // J17
    { ENC5_MCP_CLK, ENC5_MCP_DT, ENC5_MCP_SW },  // J18
};

// =============================================================================
// State
// =============================================================================

static struct {
    bool initialized;
    bool enabled;

    // I2C
    TwoWire* wire;
    uint8_t address;

    // Encoder states
    EncoderState encoders[ENCODER_MUX_MAX_ENCODERS];
    uint8_t lastState[ENCODER_MUX_MAX_ENCODERS];  // Previous CLK/DT state

    // Callback
    EncoderCallback callback;

    // Statistics
    uint32_t updateCount;
    uint16_t lastGPIO;
} state = {
    .initialized = false,
    .enabled = false,
    .wire = nullptr,
    .address = MCP23017_ADDR,
    .callback = nullptr,
    .updateCount = 0,
    .lastGPIO = 0xFFFF
};

// =============================================================================
// MCP23017 I2C Communication
// =============================================================================

static bool mcp23017WriteReg(uint8_t reg, uint8_t value) {
    state.wire->beginTransmission(state.address);
    state.wire->write(reg);
    state.wire->write(value);
    return state.wire->endTransmission() == 0;
}

static uint8_t mcp23017ReadReg(uint8_t reg) {
    state.wire->beginTransmission(state.address);
    state.wire->write(reg);
    state.wire->endTransmission();

    state.wire->requestFrom(state.address, (uint8_t)1);
    if (state.wire->available()) {
        return state.wire->read();
    }
    return 0xFF;
}

static uint16_t mcp23017ReadGPIO() {
    state.wire->beginTransmission(state.address);
    state.wire->write(MCP23017_GPIOA);
    state.wire->endTransmission();

    state.wire->requestFrom(state.address, (uint8_t)2);
    if (state.wire->available() >= 2) {
        uint8_t a = state.wire->read();
        uint8_t b = state.wire->read();
        return (b << 8) | a;
    }
    return 0xFFFF;
}

static bool mcp23017Init() {
    // Check if device responds
    state.wire->beginTransmission(state.address);
    if (state.wire->endTransmission() != 0) {
        ESP_LOGE(TAG, "MCP23017 not found at address 0x%02X", state.address);
        return false;
    }

    // Configure all pins as inputs (default)
    mcp23017WriteReg(MCP23017_IODIRA, 0xFF);  // All inputs
    mcp23017WriteReg(MCP23017_IODIRB, 0xFF);  // All inputs

    // Enable internal pull-ups on all pins
    mcp23017WriteReg(MCP23017_GPPUA, 0xFF);
    mcp23017WriteReg(MCP23017_GPPUB, 0xFF);

    // Configure IOCON
    // Bit 6 (MIRROR): Mirror interrupts (INTA = INTB)
    // Bit 2 (ODR): Open-drain output for INT pins
    mcp23017WriteReg(MCP23017_IOCON, 0x44);

    ESP_LOGI(TAG, "MCP23017 initialized at address 0x%02X", state.address);
    return true;
}

// =============================================================================
// Encoder State Machine
// =============================================================================

// Gray code state machine for encoder
// Returns: 1 for CW, -1 for CCW, 0 for no change
static int8_t decodeEncoder(uint8_t encoderIndex, uint8_t newState) {
    static const int8_t stateTable[16] = {
        0, -1,  1,  0,
        1,  0,  0, -1,
       -1,  0,  0,  1,
        0,  1, -1,  0
    };

    uint8_t oldState = state.lastState[encoderIndex];
    state.lastState[encoderIndex] = newState;

    uint8_t transition = (oldState << 2) | newState;
    return stateTable[transition];
}

// =============================================================================
// Public API
// =============================================================================

bool encoderMuxInit() {
    if (state.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return true;
    }

    ESP_LOGI(TAG, "Initializing encoder multiplexer");
    ESP_LOGI(TAG, "I2C: SDA=%d, SCL=%d, Address=0x%02X",
             I2C_MASTER_SDA_PIN, I2C_MASTER_SCL_PIN, MCP23017_ADDR);

    // Initialize I2C
    state.wire = &Wire;
    state.wire->begin(I2C_MASTER_SDA_PIN, I2C_MASTER_SCL_PIN, I2C_MASTER_FREQ);

    // Initialize MCP23017
    if (!mcp23017Init()) {
        return false;
    }

    // Initialize encoder states
    for (int i = 0; i < ENCODER_MUX_MAX_ENCODERS; i++) {
        state.encoders[i].position = 0;
        state.encoders[i].minValue = INT32_MIN;
        state.encoders[i].maxValue = INT32_MAX;
        state.encoders[i].stepSize = 1;
        state.encoders[i].buttonState = false;
        state.encoders[i].buttonChanged = false;
        state.encoders[i].enabled = true;
        state.lastState[i] = 0x03;  // Both high (pull-ups)
    }

    // Configure power steering encoder with defaults
    encoderMuxConfigure(ENCODER_POWER_STEERING,
                        POWER_STEERING_MIN,
                        POWER_STEERING_MAX,
                        POWER_STEERING_STEP,
                        POWER_STEERING_DEFAULT);

    // Read initial GPIO state
    state.lastGPIO = mcp23017ReadGPIO();

    state.initialized = true;
    state.enabled = false;

    return true;
}

void encoderMuxEnable() {
    if (!state.initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return;
    }

    if (state.enabled) {
        ESP_LOGW(TAG, "Already enabled");
        return;
    }

    // Read current state to initialize
    state.lastGPIO = mcp23017ReadGPIO();

    // Initialize encoder states from current GPIO
    for (int i = 0; i < ENCODER_MUX_MAX_ENCODERS; i++) {
        uint8_t clk = (state.lastGPIO >> encoderPins[i].clkPin) & 1;
        uint8_t dt = (state.lastGPIO >> encoderPins[i].dtPin) & 1;
        state.lastState[i] = (clk << 1) | dt;

        uint8_t sw = (state.lastGPIO >> encoderPins[i].swPin) & 1;
        state.encoders[i].buttonState = (sw == 0);  // Active low
    }

    state.enabled = true;
    ESP_LOGI(TAG, "Encoder multiplexer enabled");
}

void encoderMuxDisable() {
    if (!state.enabled) {
        return;
    }

    state.enabled = false;
    ESP_LOGI(TAG, "Encoder multiplexer disabled");
}

bool encoderMuxIsEnabled() {
    return state.enabled;
}

void encoderMuxConfigure(uint8_t encoderIndex, int32_t minValue, int32_t maxValue,
                         int32_t stepSize, int32_t initialValue) {
    if (encoderIndex >= ENCODER_MUX_MAX_ENCODERS) {
        ESP_LOGE(TAG, "Invalid encoder index: %d", encoderIndex);
        return;
    }

    EncoderState* enc = &state.encoders[encoderIndex];
    enc->minValue = minValue;
    enc->maxValue = maxValue;
    enc->stepSize = stepSize;
    enc->position = initialValue;

    ESP_LOGI(TAG, "Encoder %d configured: min=%d, max=%d, step=%d, initial=%d",
             encoderIndex, minValue, maxValue, stepSize, initialValue);
}

void encoderMuxSetEnabled(uint8_t encoderIndex, bool enabled) {
    if (encoderIndex >= ENCODER_MUX_MAX_ENCODERS) {
        return;
    }
    state.encoders[encoderIndex].enabled = enabled;
}

void encoderMuxSetCallback(EncoderCallback callback) {
    state.callback = callback;
}

bool encoderMuxUpdate() {
    if (!state.enabled) {
        return false;
    }

    // Read current GPIO state
    uint16_t gpio = mcp23017ReadGPIO();
    if (gpio == state.lastGPIO) {
        return false;  // No change
    }

    bool anyChange = false;
    state.updateCount++;

    for (int i = 0; i < ENCODER_MUX_MAX_ENCODERS; i++) {
        if (!state.encoders[i].enabled) {
            continue;
        }

        EncoderState* enc = &state.encoders[i];
        const EncoderPins* pins = &encoderPins[i];

        // Read encoder pins
        uint8_t clk = (gpio >> pins->clkPin) & 1;
        uint8_t dt = (gpio >> pins->dtPin) & 1;
        uint8_t sw = (gpio >> pins->swPin) & 1;

        // Decode rotation
        uint8_t newState = (clk << 1) | dt;
        int8_t direction = decodeEncoder(i, newState);

        if (direction != 0) {
            anyChange = true;

            // Update position with bounds checking
            int32_t newPos = enc->position + (direction * enc->stepSize);
            if (newPos < enc->minValue) {
                newPos = enc->minValue;
            } else if (newPos > enc->maxValue) {
                newPos = enc->maxValue;
            }
            enc->position = newPos;

            // Fire callback
            if (state.callback) {
                EncoderEvent event = (direction > 0) ? ENCODER_EVENT_CW : ENCODER_EVENT_CCW;
                state.callback(i, event, enc->position);
            }
        }

        // Check button state (active low)
        bool buttonPressed = (sw == 0);
        if (buttonPressed != enc->buttonState) {
            anyChange = true;
            enc->buttonState = buttonPressed;
            enc->buttonChanged = true;

            // Fire callback
            if (state.callback) {
                EncoderEvent event = buttonPressed ? ENCODER_EVENT_BUTTON_DOWN : ENCODER_EVENT_BUTTON_UP;
                state.callback(i, event, enc->position);
            }
        }
    }

    state.lastGPIO = gpio;
    return anyChange;
}

int32_t encoderMuxGetPosition(uint8_t encoderIndex) {
    if (encoderIndex >= ENCODER_MUX_MAX_ENCODERS) {
        return 0;
    }
    return state.encoders[encoderIndex].position;
}

void encoderMuxSetPosition(uint8_t encoderIndex, int32_t position) {
    if (encoderIndex >= ENCODER_MUX_MAX_ENCODERS) {
        return;
    }

    EncoderState* enc = &state.encoders[encoderIndex];

    // Clamp to bounds
    if (position < enc->minValue) {
        position = enc->minValue;
    } else if (position > enc->maxValue) {
        position = enc->maxValue;
    }

    enc->position = position;
}

bool encoderMuxGetButton(uint8_t encoderIndex) {
    if (encoderIndex >= ENCODER_MUX_MAX_ENCODERS) {
        return false;
    }
    return state.encoders[encoderIndex].buttonState;
}

bool encoderMuxButtonPressed(uint8_t encoderIndex) {
    if (encoderIndex >= ENCODER_MUX_MAX_ENCODERS) {
        return false;
    }

    EncoderState* enc = &state.encoders[encoderIndex];
    if (enc->buttonChanged && enc->buttonState) {
        enc->buttonChanged = false;
        return true;
    }
    return false;
}

EncoderState* encoderMuxGetState(uint8_t encoderIndex) {
    if (encoderIndex >= ENCODER_MUX_MAX_ENCODERS) {
        return nullptr;
    }
    return &state.encoders[encoderIndex];
}

// =============================================================================
// Power Steering Convenience Functions
// =============================================================================

uint8_t encoderMuxGetPowerSteeringLevel() {
    int32_t pos = encoderMuxGetPosition(ENCODER_POWER_STEERING);

    // Clamp to 0-100 range
    if (pos < 0) pos = 0;
    if (pos > 100) pos = 100;

    return (uint8_t)pos;
}

void encoderMuxSetPowerSteeringLevel(uint8_t level) {
    if (level > 100) level = 100;
    encoderMuxSetPosition(ENCODER_POWER_STEERING, (int32_t)level);
}

// =============================================================================
// Diagnostics
// =============================================================================

bool encoderMuxIsConnected() {
    if (!state.initialized || state.wire == nullptr) {
        return false;
    }

    state.wire->beginTransmission(state.address);
    return state.wire->endTransmission() == 0;
}

uint16_t encoderMuxGetRawGPIO() {
    if (!state.initialized) {
        return 0xFFFF;
    }
    return mcp23017ReadGPIO();
}

uint32_t encoderMuxGetUpdateCount() {
    return state.updateCount;
}
