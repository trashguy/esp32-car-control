#include "screen_settings.h"
#include "../display_common.h"
#include "../../sd_card.h"
#include "../../usb_msc.h"
#include "shared/version.h"
#include <Arduino.h>
#include <WiFi.h>

// =============================================================================
// Local State
// =============================================================================

static int diagScrollOffset = 0;
static TouchState touchState = {false, -1, false};

// Button states
static bool backButtonPressed = false;
static bool sdButtonPressed = false;
static bool wifiButtonPressed = false;
#if PRODUCTION_BUILD
static bool usbButtonPressed = false;
#endif

// WiFi SSID storage for display
extern char wifiSsid[];  // Defined in screen_wifi.cpp

// =============================================================================
// Drawing Functions
// =============================================================================

static void drawBackButton(bool pressed) {
    drawButton(BACK_BTN_X, BACK_BTN_Y, BACK_BTN_WIDTH, BACK_BTN_HEIGHT, "BACK", pressed);
}

static void drawSdButton(bool pressed) {
    TFT_eSPI& tft = getTft();
    uint16_t btnColor = pressed ? COLOR_BTN_PRESSED : COLOR_BTN_NORMAL;
    int16_t cx = SD_BTN_X + SD_BTN_SIZE / 2;
    int16_t cy = SD_BTN_Y + SD_BTN_SIZE / 2;

    tft.fillRoundRect(SD_BTN_X, SD_BTN_Y, SD_BTN_SIZE, SD_BTN_SIZE, 6, btnColor);
    tft.drawRoundRect(SD_BTN_X, SD_BTN_Y, SD_BTN_SIZE, SD_BTN_SIZE, 6, COLOR_BTN_TEXT);

    drawSdCardIcon(cx, cy, SD_BTN_SIZE - 8, COLOR_BTN_TEXT);
}

static void drawWifiButton(bool pressed) {
    TFT_eSPI& tft = getTft();
    uint16_t btnColor = pressed ? COLOR_BTN_PRESSED : COLOR_BTN_NORMAL;
    int16_t cx = SETTINGS_WIFI_BTN_X + SETTINGS_WIFI_BTN_SIZE / 2;
    int16_t cy = SETTINGS_WIFI_BTN_Y + SETTINGS_WIFI_BTN_SIZE / 2;

    tft.fillRoundRect(SETTINGS_WIFI_BTN_X, SETTINGS_WIFI_BTN_Y,
                      SETTINGS_WIFI_BTN_SIZE, SETTINGS_WIFI_BTN_SIZE, 6, btnColor);
    tft.drawRoundRect(SETTINGS_WIFI_BTN_X, SETTINGS_WIFI_BTN_Y,
                      SETTINGS_WIFI_BTN_SIZE, SETTINGS_WIFI_BTN_SIZE, 6, COLOR_BTN_TEXT);

    drawWifiIcon(cx, cy, SETTINGS_WIFI_BTN_SIZE - 12, COLOR_BTN_TEXT);
}

#if PRODUCTION_BUILD
static void drawUsbButton(bool pressed) {
    TFT_eSPI& tft = getTft();
    bool enabled = usbMscIsEnabled();
    
    // Use different colors based on enabled state
    uint16_t btnColor;
    uint16_t iconColor;
    
    if (pressed) {
        btnColor = COLOR_BTN_PRESSED;
        iconColor = COLOR_BTN_TEXT;
    } else if (enabled) {
        btnColor = COLOR_CONNECTED;  // Green when enabled
        iconColor = COLOR_BACKGROUND;
    } else {
        btnColor = COLOR_BTN_NORMAL;
        iconColor = COLOR_BTN_TEXT;
    }
    
    int16_t cx = USB_BTN_X + USB_BTN_SIZE / 2;
    int16_t cy = USB_BTN_Y + USB_BTN_SIZE / 2;

    tft.fillRoundRect(USB_BTN_X, USB_BTN_Y, USB_BTN_SIZE, USB_BTN_SIZE, 6, btnColor);
    tft.drawRoundRect(USB_BTN_X, USB_BTN_Y, USB_BTN_SIZE, USB_BTN_SIZE, 6, COLOR_BTN_TEXT);

    // Draw USB icon (simple representation)
    // USB connector shape
    tft.fillRoundRect(cx - 8, cy - 6, 16, 10, 2, iconColor);
    tft.fillRect(cx - 5, cy + 4, 10, 4, iconColor);
    // USB pins
    tft.fillRect(cx - 4, cy - 3, 2, 4, btnColor);
    tft.fillRect(cx + 2, cy - 3, 2, 4, btnColor);
}
#endif // PRODUCTION_BUILD

static void drawDiagnosticsContent() {
    TFT_eSPI& tft = getTft();

    // Clear content area
    tft.fillRect(0, DIAG_CONTENT_Y, SCREEN_WIDTH, DIAG_CONTENT_H, COLOR_BACKGROUND);

    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(1);
    int16_t baseY = DIAG_CONTENT_Y + 5;
    int16_t lineHeight = 18;
    char buf[64];

    // Macro to draw a line with scrolling support
    #define DRAW_LINE(label, value, valueColor) do { \
        int16_t screenY = baseY - diagScrollOffset; \
        if (screenY >= DIAG_CONTENT_Y && screenY + lineHeight <= DIAG_CONTENT_Y + DIAG_CONTENT_H) { \
            tft.setTextColor(COLOR_LABEL, COLOR_BACKGROUND); \
            tft.drawString(label, 10, screenY); \
            tft.setTextColor(valueColor, COLOR_BACKGROUND); \
            tft.drawString(value, 120, screenY); \
        } \
        baseY += lineHeight; \
    } while(0)

    #define DRAW_SEPARATOR() do { \
        int16_t screenY = baseY - diagScrollOffset; \
        if (screenY >= DIAG_CONTENT_Y && screenY + 2 <= DIAG_CONTENT_Y + DIAG_CONTENT_H) { \
            tft.drawLine(20, screenY, SCREEN_WIDTH - 20, screenY, COLOR_LABEL); \
        } \
        baseY += 8; \
    } while(0)

    // Firmware section
    DRAW_LINE("Firmware:", FIRMWARE_VERSION, COLOR_RPM_TEXT);
    DRAW_LINE("Built:", BUILD_TIMESTAMP, COLOR_RPM_TEXT);
    
    baseY += 5;
    DRAW_SEPARATOR();

    // SD Card section
    if (sdCardPresent()) {
        DRAW_LINE("SD Card:", sdCardType(), COLOR_CONNECTED);

        snprintf(buf, sizeof(buf), "%llu MB", sdCardTotalBytes() / (1024 * 1024));
        DRAW_LINE("Total:", buf, COLOR_RPM_TEXT);

        snprintf(buf, sizeof(buf), "%llu MB", sdCardUsedBytes() / (1024 * 1024));
        DRAW_LINE("Used:", buf, COLOR_RPM_TEXT);
    } else {
        DRAW_LINE("SD Card:", "Not Present", COLOR_DISCONNECTED);
    }

    baseY += 5;
    DRAW_SEPARATOR();

    // WiFi section
    int wifiMode = getWifiMode();
    const char* modeStr = (wifiMode == 0) ? "Disabled" : "Client";
    uint16_t modeColor = (wifiMode == 0) ? COLOR_DISCONNECTED : COLOR_CONNECTED;
    DRAW_LINE("WiFi Mode:", modeStr, modeColor);

    if (wifiMode == 1) {
        bool connected = (WiFi.status() == WL_CONNECTED);
        DRAW_LINE("Status:", connected ? "Connected" : "Disconnected",
                  connected ? COLOR_CONNECTED : COLOR_DISCONNECTED);

        if (connected) {
            DRAW_LINE("SSID:", wifiSsid, COLOR_RPM_TEXT);

            IPAddress ip = WiFi.localIP();
            snprintf(buf, sizeof(buf), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
            DRAW_LINE("IP Address:", buf, COLOR_RPM_TEXT);

            snprintf(buf, sizeof(buf), "%d dBm", WiFi.RSSI());
            DRAW_LINE("Signal:", buf, COLOR_RPM_TEXT);
        } else if (strlen(wifiSsid) > 0) {
            DRAW_LINE("SSID:", wifiSsid, COLOR_WARNING);
            DRAW_LINE("Status:", "Connecting...", COLOR_WARNING);
        }
    }

    #undef DRAW_LINE
    #undef DRAW_SEPARATOR

    // Draw scroll indicator if content overflows
    int totalHeight = baseY - (DIAG_CONTENT_Y + 5);
    int maxScroll = totalHeight - DIAG_CONTENT_H;
    if (maxScroll > 0) {
        int16_t scrollBarX = SCREEN_WIDTH - 8;
        int16_t scrollBarY = DIAG_CONTENT_Y;
        int16_t scrollBarH = DIAG_CONTENT_H;
        int16_t thumbH = (DIAG_CONTENT_H * DIAG_CONTENT_H) / totalHeight;
        if (thumbH < 20) thumbH = 20;
        int16_t thumbY = scrollBarY + (diagScrollOffset * (scrollBarH - thumbH)) / maxScroll;

        tft.fillRect(scrollBarX, scrollBarY, 4, scrollBarH, COLOR_BTN_NORMAL);
        tft.fillRect(scrollBarX, thumbY, 4, thumbH, COLOR_BTN_TEXT);
    }
}

// =============================================================================
// Public Interface
// =============================================================================

void screenSettingsDraw() {
    TFT_eSPI& tft = getTft();
    tft.fillScreen(COLOR_BACKGROUND);

    // Title
    tft.setTextColor(COLOR_LABEL, COLOR_BACKGROUND);
    tft.setTextDatum(TC_DATUM);
    tft.setTextSize(2);
    tft.drawString("SETTINGS", SCREEN_WIDTH / 2, 10);

    // Horizontal line
    tft.drawLine(20, 35, SCREEN_WIDTH - 20, 35, COLOR_LABEL);

    // Draw scrollable content
    drawDiagnosticsContent();

    // Draw buttons
    drawBackButton(false);
    drawWifiButton(false);

    // Only show SD and USB buttons if card present
    if (sdCardPresent()) {
        drawSdButton(false);
#if PRODUCTION_BUILD
        drawUsbButton(false);
#endif
    }

    drawWifiStatusIndicator();
}

void screenSettingsHandleTouch(int16_t x, int16_t y, bool pressed) {
    if (pressed) {
        // Check back button
        if (pointInRect(x, y, BACK_BTN_X, BACK_BTN_Y, BACK_BTN_WIDTH, BACK_BTN_HEIGHT) &&
            !backButtonPressed) {
            backButtonPressed = true;
            drawBackButton(true);
            Serial.println("BACK button pressed");
        }

        // Check SD card button
        if (sdCardPresent() &&
            pointInRect(x, y, SD_BTN_X, SD_BTN_Y, SD_BTN_SIZE, SD_BTN_SIZE) &&
            !sdButtonPressed) {
            sdButtonPressed = true;
            drawSdButton(true);
            Serial.println("SD button pressed");
        }

        // Check WiFi button
        if (pointInRect(x, y, SETTINGS_WIFI_BTN_X, SETTINGS_WIFI_BTN_Y,
                       SETTINGS_WIFI_BTN_SIZE, SETTINGS_WIFI_BTN_SIZE) &&
            !wifiButtonPressed) {
            wifiButtonPressed = true;
            drawWifiButton(true);
            Serial.println("WIFI button pressed");
        }

        // Check USB button (production build only)
#if PRODUCTION_BUILD
        if (sdCardPresent() &&
            pointInRect(x, y, USB_BTN_X, USB_BTN_Y, USB_BTN_SIZE, USB_BTN_SIZE) &&
            !usbButtonPressed) {
            usbButtonPressed = true;
            drawUsbButton(true);
            Serial.println("USB button pressed");
        }
#endif

        // Handle scrolling in content area
        if (y >= DIAG_CONTENT_Y && y < DIAG_CONTENT_Y + DIAG_CONTENT_H) {
            if (!touchState.isDragging) {
                touchState.isDragging = true;
                touchState.lastTouchY = y;
            } else if (touchState.lastTouchY >= 0) {
                int16_t delta = touchState.lastTouchY - y;
                if (abs(delta) > 3) {
                    int maxScroll = 80;
                    diagScrollOffset += delta;
                    if (diagScrollOffset < 0) diagScrollOffset = 0;
                    if (diagScrollOffset > maxScroll) diagScrollOffset = maxScroll;
                    drawDiagnosticsContent();
                    touchState.lastTouchY = y;
                }
            }
        }
    } else {
        // Handle releases
        if (backButtonPressed) {
            backButtonPressed = false;
            diagScrollOffset = 0;
            switchToScreen(SCREEN_MAIN);
            Serial.println("Switching to main screen");
        }

        if (sdButtonPressed) {
            sdButtonPressed = false;
            switchToScreen(SCREEN_FILE_BROWSER);
            Serial.println("Switching to file browser screen");
        }

        if (wifiButtonPressed) {
            wifiButtonPressed = false;
            switchToScreen(SCREEN_WIFI);
            Serial.println("Switching to WiFi screen");
        }

#if PRODUCTION_BUILD
        if (usbButtonPressed) {
            usbButtonPressed = false;
            // Toggle USB MSC state
            if (usbMscIsEnabled()) {
                usbMscDisable();
                Serial.println("USB Mass Storage disabled");
            } else {
                usbMscEnable();
                Serial.println("USB Mass Storage enabled");
            }
            drawUsbButton(false);  // Redraw with new state
        }
#endif

        touchState.isDragging = false;
        touchState.lastTouchY = -1;
    }
}

// Track last known USB state to detect changes
#if PRODUCTION_BUILD
static bool lastUsbEnabled = false;
#endif

void screenSettingsUpdate() {
#if PRODUCTION_BUILD
    // Check if USB state changed (e.g., host ejected) and update button
    bool currentUsbEnabled = usbMscIsEnabled();
    if (currentUsbEnabled != lastUsbEnabled) {
        lastUsbEnabled = currentUsbEnabled;
        drawUsbButton(false);
    }
#endif
}

void screenSettingsReset() {
    diagScrollOffset = 0;
    touchState = {false, -1, false};
    backButtonPressed = false;
    sdButtonPressed = false;
    wifiButtonPressed = false;
#if PRODUCTION_BUILD
    usbButtonPressed = false;
    lastUsbEnabled = usbMscIsEnabled();  // Sync with current state
#endif
}
