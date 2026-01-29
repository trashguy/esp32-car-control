#include "display.h"
#include "display_common.h"
#include "tasks.h"
#include "slave/spi_slave.h"
#include "screen_main.h"
#include "screen_settings.h"
#include "screen_filebrowser.h"
#include "screen_wifi.h"
#include "shared/config.h"
#include "shared/protocol.h"
#include <Arduino.h>

// =============================================================================
// Screen Management
// =============================================================================

static ScreenType currentScreen = SCREEN_MAIN;
static bool lastTouchState = false;
static unsigned long lastWifiStatusCheck = 0;
static bool lastWifiConnected = false;

ScreenType getCurrentScreen() {
    return currentScreen;
}

void switchToScreen(ScreenType screen) {
    // Reset current screen state before switching
    switch (currentScreen) {
        case SCREEN_MAIN:
            // Main screen has no reset needed
            break;
        case SCREEN_SETTINGS:
            screenSettingsReset();
            break;
        case SCREEN_FILE_BROWSER:
            screenFileBrowserReset();
            break;
        case SCREEN_WIFI:
            screenWifiReset();
            break;
    }

    currentScreen = screen;

    // Draw the new screen
    switch (screen) {
        case SCREEN_MAIN:
            screenMainDraw();
            break;
        case SCREEN_SETTINGS:
            screenSettingsDraw();
            break;
        case SCREEN_FILE_BROWSER:
            screenFileBrowserDraw();
            break;
        case SCREEN_WIFI:
            screenWifiDraw();
            break;
    }
}

// =============================================================================
// Display Initialization
// =============================================================================

bool displayInit() {
    TFT_eSPI& tft = getTft();

    // Initialize TFT
    tft.init();
    tft.setRotation(1);  // Landscape
    tft.invertDisplay(true);  // ILI9341V requires color inversion
    tft.fillScreen(COLOR_BACKGROUND);

    // Configure backlight
    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, HIGH);

    Serial.println("ILI9341V display initialized (320x240, Rotation 1)");

    // Initialize touch controller
    if (!touchInit()) {
        Serial.println("WARNING: Touch controller not available");
    }

    // Initialize WiFi settings (loads from NVS)
    screenWifiInit();

    // Initialize main screen state
    screenMainInit();

    // Draw initial screen
    screenMainDraw();

    Serial.println("Touch controller uses Wire (I2C0), RPM slave uses SPI");

    return true;
}

// =============================================================================
// Public API - Called from main.cpp
// =============================================================================

void displayUpdateRpm(uint16_t rpm) {
    screenMainUpdateRpm(rpm, true);
}

void displaySetConnected(bool connected) {
    screenMainSetConnected(connected);
}

bool displayGetTouch(int16_t* x, int16_t* y) {
    return touchGetPoint(x, y);
}

ScreenType displayGetScreen() {
    return currentScreen;
}

// =============================================================================
// Main Display Loop
// =============================================================================

void displayLoop() {
    // Periodic WiFi status check (not during keyboard input)
    if (!screenWifiKeyboardVisible() && millis() - lastWifiStatusCheck > 1000) {
        lastWifiStatusCheck = millis();
        bool currentWifiConnected = isWifiConnected();
        if (currentWifiConnected != lastWifiConnected) {
            lastWifiConnected = currentWifiConnected;
            drawWifiStatusIndicator();
        }
    }

    // Get touch input
    int16_t touchX, touchY;
    bool currentTouch = touchGetPoint(&touchX, &touchY);

    // Dispatch to current screen
    switch (currentScreen) {
        case SCREEN_MAIN:
            screenMainUpdate();
            if (currentTouch != lastTouchState || currentTouch) {
                screenMainHandleTouch(touchX, touchY, currentTouch);
            }
            break;

        case SCREEN_SETTINGS:
            screenSettingsUpdate();
            if (currentTouch != lastTouchState || currentTouch) {
                screenSettingsHandleTouch(touchX, touchY, currentTouch);
            }
            break;

        case SCREEN_FILE_BROWSER:
            screenFileBrowserUpdate();
            if (currentTouch != lastTouchState || currentTouch) {
                screenFileBrowserHandleTouch(touchX, touchY, currentTouch);
            }
            break;

        case SCREEN_WIFI:
            screenWifiUpdate();
            // WiFi screen handles its own lastTouchState for keyboard
            screenWifiHandleTouch(touchX, touchY, currentTouch);
            break;
    }

    lastTouchState = currentTouch;
}

// =============================================================================
// Thread-Safe UI Command Sending
// =============================================================================

bool displaySendModeRequest(uint8_t mode) {
    if (queueDisplayToSpi == nullptr) {
        // Queue not initialized - fall back to direct call
        spiSlaveSetRequestedMode(mode);
        return true;
    }

    DisplayToSpiMsg msg;
    msg.requestedMode = mode;
    msg.requestedRpm = spiSlaveGetRequestedRpm();

    return xQueueSend(queueDisplayToSpi, &msg, pdMS_TO_TICKS(10)) == pdTRUE;
}

bool displaySendRpmRequest(uint16_t rpm) {
    if (queueDisplayToSpi == nullptr) {
        // Queue not initialized - fall back to direct call
        spiSlaveSetRequestedRpm(rpm);
        return true;
    }

    DisplayToSpiMsg msg;
    msg.requestedMode = spiSlaveGetRequestedMode();
    msg.requestedRpm = rpm;

    return xQueueSend(queueDisplayToSpi, &msg, pdMS_TO_TICKS(10)) == pdTRUE;
}

bool displaySendRequest(uint8_t mode, uint16_t rpm) {
    if (queueDisplayToSpi == nullptr) {
        // Queue not initialized - fall back to direct call
        spiSlaveSetRequest(mode, rpm);
        return true;
    }

    DisplayToSpiMsg msg;
    msg.requestedMode = mode;
    msg.requestedRpm = rpm;

    return xQueueSend(queueDisplayToSpi, &msg, pdMS_TO_TICKS(10)) == pdTRUE;
}
