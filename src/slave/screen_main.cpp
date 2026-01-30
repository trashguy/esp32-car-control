#include "screen_main.h"
#include "slave/spi_slave.h"
#include "shared/config.h"
#include "shared/protocol.h"
#include <Arduino.h>

// =============================================================================
// Local State
// =============================================================================

static DisplayState currentState = DISPLAY_NO_SIGNAL;
static uint16_t displayedRpm = 0;
static uint16_t manualRpm = 3000;

static unsigned long lastBlinkTime = 0;
static bool blinkState = false;
static bool lastSyncState = false;

// Button states
static bool gearButtonPressed = false;
static bool modeButtonPressed = false;
static bool rpmUpButtonPressed = false;
static bool rpmDownButtonPressed = false;

// =============================================================================
// Helper Functions
// =============================================================================

static bool isSynced() {
    if (!spiSlaveIsConnected()) return false;

    uint8_t masterMode = spiSlaveGetMasterMode();
    uint8_t requestedMode = spiSlaveGetRequestedMode();

    if (masterMode != requestedMode) {
        return false;
    }

    if (masterMode == MODE_MANUAL) {
        uint16_t masterRpm = spiSlaveGetLastRpm();
        uint16_t requestedRpm = spiSlaveGetRequestedRpm();
        return masterRpm == requestedRpm;
    }

    return true;
}

// =============================================================================
// Drawing Functions
// =============================================================================

static void drawBackground() {
    TFT_eSPI& tft = getTft();
    tft.fillScreen(COLOR_BACKGROUND);

    tft.setTextColor(COLOR_LABEL, COLOR_BACKGROUND);
    tft.setTextDatum(TC_DATUM);
    tft.setTextSize(2);
    tft.drawString("POWER STEERING", SCREEN_WIDTH / 2, 10);
}

static void drawRpmUpButton(bool pressed) {
    TFT_eSPI& tft = getTft();
    uint16_t btnColor = pressed ? COLOR_BTN_PRESSED : COLOR_BTN_NORMAL;
    int16_t cx = RPM_UP_BTN_X + RPM_BTN_SIZE / 2;
    int16_t cy = RPM_BTN_Y + RPM_BTN_SIZE / 2;

    tft.fillRoundRect(RPM_UP_BTN_X, RPM_BTN_Y, RPM_BTN_SIZE, RPM_BTN_SIZE, 6, btnColor);
    tft.drawRoundRect(RPM_UP_BTN_X, RPM_BTN_Y, RPM_BTN_SIZE, RPM_BTN_SIZE, 6, COLOR_BTN_TEXT);

    // Draw up arrow triangle
    int16_t arrowSize = 12;
    tft.fillTriangle(
        cx, cy - arrowSize,
        cx - arrowSize, cy + arrowSize / 2,
        cx + arrowSize, cy + arrowSize / 2,
        COLOR_BTN_TEXT
    );
}

static void drawRpmDownButton(bool pressed) {
    TFT_eSPI& tft = getTft();
    uint16_t btnColor = pressed ? COLOR_BTN_PRESSED : COLOR_BTN_NORMAL;
    int16_t cx = RPM_DOWN_BTN_X + RPM_BTN_SIZE / 2;
    int16_t cy = RPM_BTN_Y + RPM_BTN_SIZE / 2;

    tft.fillRoundRect(RPM_DOWN_BTN_X, RPM_BTN_Y, RPM_BTN_SIZE, RPM_BTN_SIZE, 6, btnColor);
    tft.drawRoundRect(RPM_DOWN_BTN_X, RPM_BTN_Y, RPM_BTN_SIZE, RPM_BTN_SIZE, 6, COLOR_BTN_TEXT);

    // Draw down arrow triangle
    int16_t arrowSize = 12;
    tft.fillTriangle(
        cx, cy + arrowSize,
        cx - arrowSize, cy - arrowSize / 2,
        cx + arrowSize, cy - arrowSize / 2,
        COLOR_BTN_TEXT
    );
}

static void drawRpmValue(uint16_t rpm, bool connected) {
    TFT_eSPI& tft = getTft();

    // Clear RPM area
    tft.fillRect(0, RPM_Y_POS - 40, SCREEN_WIDTH, 80, COLOR_BACKGROUND);

    tft.setTextDatum(MC_DATUM);

    if (!connected) {
        tft.setTextColor(COLOR_DISCONNECTED, COLOR_BACKGROUND);
        tft.setTextSize(3);
        tft.drawString("NO SIGNAL", SCREEN_WIDTH / 2, RPM_Y_POS);
    } else {
        tft.setTextColor(COLOR_RPM_TEXT, COLOR_BACKGROUND);
        tft.setTextSize(6);

        char rpmStr[8];
        snprintf(rpmStr, sizeof(rpmStr), "%u", rpm);
        tft.drawString(rpmStr, SCREEN_WIDTH / 2, RPM_Y_POS);
    }

    // Draw up/down buttons if in manual mode and connected
    if (spiSlaveGetMasterMode() == MODE_MANUAL && connected) {
        drawRpmUpButton(rpmUpButtonPressed);
        drawRpmDownButton(rpmDownButtonPressed);
    }
}

static void drawStatusIndicator(bool connected) {
    TFT_eSPI& tft = getTft();

    // Clear sync dot area
    tft.fillCircle(SYNC_DOT_X, SYNC_DOT_Y, SYNC_DOT_R + 1, COLOR_BACKGROUND);

    if (connected) {
        bool synced = isSynced();
        uint16_t dotColor = synced ? COLOR_CONNECTED : COLOR_WARNING;
        tft.fillCircle(SYNC_DOT_X, SYNC_DOT_Y, SYNC_DOT_R, dotColor);
    } else {
        uint16_t dotColor = blinkState ? COLOR_DISCONNECTED : COLOR_BACKGROUND;
        tft.fillCircle(SYNC_DOT_X, SYNC_DOT_Y, SYNC_DOT_R, dotColor);
    }
}

static void drawVonderwagen3DLogo() {
    TFT_eSPI& tft = getTft();
    const char* logoText = "Vonderwagen";
    int16_t centerX = SCREEN_WIDTH / 2;

    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);

    // Draw shadow for 3D effect
    tft.setTextColor(0x4228, COLOR_BACKGROUND);
    tft.drawString(logoText, centerX + 2, LOGO_Y_POS + 2);

    // Main text
    tft.setTextColor(0xFFFF, COLOR_BACKGROUND);
    tft.drawString(logoText, centerX, LOGO_Y_POS);
}

static void drawGearButton(bool pressed) {
    TFT_eSPI& tft = getTft();
    uint16_t btnColor = pressed ? COLOR_BTN_PRESSED : COLOR_BTN_NORMAL;
    int16_t cx = GEAR_BTN_X + GEAR_BTN_SIZE / 2;
    int16_t cy = GEAR_BTN_Y + GEAR_BTN_SIZE / 2;

    tft.fillRoundRect(GEAR_BTN_X, GEAR_BTN_Y, GEAR_BTN_SIZE, GEAR_BTN_SIZE, 6, btnColor);
    tft.drawRoundRect(GEAR_BTN_X, GEAR_BTN_Y, GEAR_BTN_SIZE, GEAR_BTN_SIZE, 6, COLOR_BTN_TEXT);

    drawGearIcon(cx, cy, GEAR_BTN_SIZE, COLOR_BTN_TEXT);
}

static void drawModeButton(bool pressed) {
    TFT_eSPI& tft = getTft();
    uint16_t btnColor = pressed ? COLOR_BTN_PRESSED : COLOR_BTN_NORMAL;
    uint8_t mode = spiSlaveGetRequestedMode();

    tft.fillRoundRect(MODE_BTN_X, MODE_BTN_Y, MODE_BTN_SIZE, MODE_BTN_SIZE, 6, btnColor);
    tft.drawRoundRect(MODE_BTN_X, MODE_BTN_Y, MODE_BTN_SIZE, MODE_BTN_SIZE, 6, COLOR_BTN_TEXT);

    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(mode == MODE_AUTO ? COLOR_CONNECTED : COLOR_WARNING, btnColor);
    tft.drawString(mode == MODE_AUTO ? "A" : "M",
                   MODE_BTN_X + MODE_BTN_SIZE / 2, MODE_BTN_Y + MODE_BTN_SIZE / 2);
}

// =============================================================================
// Public Interface
// =============================================================================

void screenMainInit() {
    manualRpm = 3000;
    spiSlaveSetRequestedRpm(manualRpm);
    spiSlaveSetRequestedMode(MODE_AUTO);
}

void screenMainDraw() {
    drawBackground();
    drawRpmValue(displayedRpm, currentState == DISPLAY_CONNECTED);
    drawStatusIndicator(currentState == DISPLAY_CONNECTED);
    drawVonderwagen3DLogo();
    drawGearButton(gearButtonPressed);
    drawModeButton(modeButtonPressed);
    drawWifiStatusIndicator();
}

void screenMainHandleTouch(int16_t x, int16_t y, bool pressed) {
    if (pressed) {
        // Check gear button
        if (pointInRect(x, y, GEAR_BTN_X, GEAR_BTN_Y, GEAR_BTN_SIZE, GEAR_BTN_SIZE) &&
            !gearButtonPressed) {
            gearButtonPressed = true;
            drawGearButton(true);
            Serial.println("GEAR button pressed");
        }

        // Check mode button
        if (pointInRect(x, y, MODE_BTN_X, MODE_BTN_Y, MODE_BTN_SIZE, MODE_BTN_SIZE) &&
            !modeButtonPressed) {
            modeButtonPressed = true;
            drawModeButton(true);
            Serial.println("MODE button pressed");
        }

        // Check RPM up/down buttons (only in manual mode)
        if (spiSlaveGetMasterMode() == MODE_MANUAL && currentState == DISPLAY_CONNECTED) {
            if (pointInRect(x, y, RPM_UP_BTN_X, RPM_BTN_Y, RPM_BTN_SIZE, RPM_BTN_SIZE) &&
                !rpmUpButtonPressed) {
                rpmUpButtonPressed = true;
                drawRpmUpButton(true);
                Serial.println("RPM UP button pressed");
            }
            if (pointInRect(x, y, RPM_DOWN_BTN_X, RPM_BTN_Y, RPM_BTN_SIZE, RPM_BTN_SIZE) &&
                !rpmDownButtonPressed) {
                rpmDownButtonPressed = true;
                drawRpmDownButton(true);
                Serial.println("RPM DOWN button pressed");
            }
        }
    } else {
        // Handle releases
        if (gearButtonPressed) {
            gearButtonPressed = false;
            switchToScreen(SCREEN_SETTINGS);
            Serial.println("Switching to settings screen");
        }

        if (modeButtonPressed) {
            modeButtonPressed = false;
            uint8_t currentMode = spiSlaveGetRequestedMode();
            uint8_t newMode = (currentMode == MODE_AUTO) ? MODE_MANUAL : MODE_AUTO;
            spiSlaveSetRequestedMode(newMode);
            drawModeButton(false);
            drawRpmValue(displayedRpm, currentState == DISPLAY_CONNECTED);
            Serial.printf("Requested mode change to %s\n", newMode == MODE_AUTO ? "AUTO" : "MANUAL");
        }

        if (rpmUpButtonPressed) {
            rpmUpButtonPressed = false;
            uint16_t currentRpm = spiSlaveGetRequestedRpm();
            if (currentRpm < 5000) {
                uint16_t newRpm = currentRpm + 100;
                manualRpm = newRpm;
                spiSlaveSetRequestedRpm(newRpm);
                Serial.printf("Requested RPM increase to %u\n", newRpm);
            } else {
                drawRpmUpButton(false);
            }
        }

        if (rpmDownButtonPressed) {
            rpmDownButtonPressed = false;
            uint16_t currentRpm = spiSlaveGetRequestedRpm();
            if (currentRpm > 500) {
                uint16_t newRpm = currentRpm - 100;
                manualRpm = newRpm;
                spiSlaveSetRequestedRpm(newRpm);
                Serial.printf("Requested RPM decrease to %u\n", newRpm);
            } else {
                drawRpmDownButton(false);
            }
        }
    }
}

void screenMainUpdate() {
    // Handle blink animation for disconnected state
    if (currentState == DISPLAY_NO_SIGNAL) {
        unsigned long now = millis();
        if (now - lastBlinkTime >= 500) {
            lastBlinkTime = now;
            blinkState = !blinkState;
            drawStatusIndicator(false);
        }
    }

    // Check if sync state changed
    bool connected = (currentState == DISPLAY_CONNECTED);
    bool currentSyncState = connected ? isSynced() : false;
    if (currentSyncState != lastSyncState) {
        lastSyncState = currentSyncState;
        drawStatusIndicator(connected);
    }
}

void screenMainUpdateRpm(uint16_t rpm, bool connected) {
    if (rpm != displayedRpm || currentState != DISPLAY_CONNECTED) {
        displayedRpm = rpm;
        currentState = DISPLAY_CONNECTED;
        if (getCurrentScreen() == SCREEN_MAIN) {
            drawRpmValue(rpm, true);
            drawStatusIndicator(true);
        }
    }
}

void screenMainSetConnected(bool connected) {
    DisplayState newState = connected ? DISPLAY_CONNECTED : DISPLAY_NO_SIGNAL;
    bool currentSyncState = connected ? isSynced() : false;

    bool stateChanged = (newState != currentState);
    bool syncChanged = (currentSyncState != lastSyncState);

    if (stateChanged || syncChanged) {
        currentState = newState;
        lastSyncState = currentSyncState;
        if (getCurrentScreen() == SCREEN_MAIN) {
            if (!connected) {
                drawRpmValue(0, false);
            }
            drawStatusIndicator(connected);
        }
    }
}
