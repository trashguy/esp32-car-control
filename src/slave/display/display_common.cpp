#include "display_common.h"
#include "../tasks.h"
#include "shared/config.h"
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <math.h>

// =============================================================================
// FT6336G Touch Controller Registers
// =============================================================================

#define FT6336G_ADDR        TOUCH_I2C_ADDR  // 0x38
#define FT6336G_REG_MODE    0x00
#define FT6336G_REG_GEST    0x01
#define FT6336G_REG_STATUS  0x02
#define FT6336G_REG_XH      0x03
#define FT6336G_REG_XL      0x04
#define FT6336G_REG_YH      0x05
#define FT6336G_REG_YL      0x06
#define FT6336G_REG_CHIPID  0xA3
#define FT6336G_REG_FWVERS  0xA6

// =============================================================================
// Global State
// =============================================================================

static TFT_eSPI tft = TFT_eSPI();
static bool touchInitialized = false;
static uint8_t touchFailCount = 0;

// WiFi state (accessed by multiple screens)
static int wifiModeState = 0;

TFT_eSPI& getTft() {
    return tft;
}

int getWifiMode() {
    return wifiModeState;
}

void setWifiMode(int mode) {
    wifiModeState = mode;
}

bool isWifiConnected() {
    return (wifiModeState == 1 && WiFi.status() == WL_CONNECTED);
}

// =============================================================================
// Touch Initialization
// =============================================================================

bool touchInit() {
    // Reset touch controller
    pinMode(TOUCH_RST_PIN, OUTPUT);
    digitalWrite(TOUCH_RST_PIN, LOW);
    delay(10);
    digitalWrite(TOUCH_RST_PIN, HIGH);
    delay(300);  // Wait for touch controller to boot

    // Wire (I2C0) is dedicated to touch controller
    Wire.begin(I2C_TOUCH_SDA_PIN, I2C_TOUCH_SCL_PIN);
    Wire.setClock(I2C_FREQUENCY);

    // Check if FT6336G is present by reading chip ID
    Wire.beginTransmission((uint8_t)FT6336G_ADDR);
    Wire.write(FT6336G_REG_CHIPID);
    if (Wire.endTransmission() == 0) {
        Wire.requestFrom((uint8_t)FT6336G_ADDR, (uint8_t)1);
        if (Wire.available()) {
            uint8_t chipId = Wire.read();
            Serial.printf("FT6336G touch controller found (Chip ID: 0x%02X)\n", chipId);

            // Read firmware version
            Wire.beginTransmission((uint8_t)FT6336G_ADDR);
            Wire.write(FT6336G_REG_FWVERS);
            Wire.endTransmission();
            Wire.requestFrom((uint8_t)FT6336G_ADDR, (uint8_t)1);
            if (Wire.available()) {
                uint8_t fwVer = Wire.read();
                Serial.printf("FT6336G firmware version: 0x%02X\n", fwVer);
            }
            touchInitialized = true;
            return true;
        }
    }

    Serial.println("FT6336G touch controller not found");
    return false;
}

bool touchGetPoint(int16_t* x, int16_t* y) {
    if (!touchInitialized) return false;

    // Take I2C mutex for thread-safe access
    if (mutexI2C != nullptr && !I2C_LOCK()) {
        return false;  // Couldn't get mutex
    }

    bool result = false;

    // Read touch data starting from register 0x02 (status)
    Wire.beginTransmission((uint8_t)FT6336G_ADDR);
    Wire.write(FT6336G_REG_STATUS);
    if (Wire.endTransmission() != 0) {
        touchFailCount++;
        if (mutexI2C != nullptr) I2C_UNLOCK();
        return false;
    }

    Wire.requestFrom((uint8_t)FT6336G_ADDR, (uint8_t)5);
    if (Wire.available() < 5) {
        touchFailCount++;
        if (mutexI2C != nullptr) I2C_UNLOCK();
        return false;
    }

    touchFailCount = 0;  // Reset on success

    uint8_t status = Wire.read();
    uint8_t touches = status & 0x0F;

    if (touches == 0 || touches > 2) {
        if (mutexI2C != nullptr) I2C_UNLOCK();
        return false;
    }

    uint8_t xh = Wire.read();
    uint8_t xl = Wire.read();
    uint8_t yh = Wire.read();
    uint8_t yl = Wire.read();

    // Extract 12-bit coordinates
    int16_t rawX = ((xh & 0x0F) << 8) | xl;
    int16_t rawY = ((yh & 0x0F) << 8) | yl;

    // Map to screen coordinates for rotation 1 (landscape)
    *x = rawY;
    *y = 240 - rawX;

    result = true;

    if (mutexI2C != nullptr) I2C_UNLOCK();
    return result;
}

// =============================================================================
// Common Drawing Utilities
// =============================================================================

void drawButton(int16_t x, int16_t y, int16_t w, int16_t h,
                const char* label, bool pressed, uint16_t textColor) {
    uint16_t btnColor = pressed ? COLOR_BTN_PRESSED : COLOR_BTN_NORMAL;

    tft.fillRoundRect(x, y, w, h, BTN_RADIUS, btnColor);
    tft.drawRoundRect(x, y, w, h, BTN_RADIUS, COLOR_BTN_TEXT);

    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(textColor, btnColor);
    tft.drawString(label, x + w / 2, y + h / 2);
}

void drawButtonColored(int16_t x, int16_t y, int16_t w, int16_t h,
                       const char* label, uint16_t bgColor, uint16_t textColor) {
    tft.fillRoundRect(x, y, w, h, BTN_RADIUS, bgColor);
    tft.drawRoundRect(x, y, w, h, BTN_RADIUS, COLOR_BTN_TEXT);

    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(textColor, bgColor);
    tft.drawString(label, x + w / 2, y + h / 2);
}

void drawGearIcon(int16_t cx, int16_t cy, int16_t size, uint16_t color) {
    int16_t outerR = size / 3;
    int16_t holeR = outerR / 3;

    // Draw outer circle (gear body)
    tft.fillCircle(cx, cy, outerR, color);

    // Draw gear teeth (6 teeth)
    for (int i = 0; i < 6; i++) {
        float angle = i * 3.14159 / 3;
        int16_t tx = cx + cos(angle) * (outerR + 2);
        int16_t ty = cy + sin(angle) * (outerR + 2);
        tft.fillCircle(tx, ty, 3, color);
    }

    // Draw center hole
    tft.fillCircle(cx, cy, holeR, COLOR_BACKGROUND);
}

void drawSdCardIcon(int16_t cx, int16_t cy, int16_t size, uint16_t color) {
    int16_t w = size * 2 / 3;
    int16_t h = size - 4;
    int16_t x = cx - w / 2;
    int16_t y = cy - h / 2;
    int16_t notch = w / 3;

    // Main body
    tft.fillRect(x, y + notch, w, h - notch, color);
    // Top part with corner cut
    tft.fillRect(x + notch, y, w - notch, notch, color);
    // Diagonal notch
    tft.fillTriangle(x, y + notch, x + notch, y + notch, x + notch, y, color);

    // Draw lines on card to show contacts
    tft.drawLine(x + 3, cy, x + 3, cy + h/3, COLOR_BACKGROUND);
    tft.drawLine(x + 6, cy, x + 6, cy + h/3, COLOR_BACKGROUND);
    tft.drawLine(x + 9, cy, x + 9, cy + h/3, COLOR_BACKGROUND);
}

void drawBackArrowIcon(int16_t cx, int16_t cy, int16_t size, uint16_t color) {
    int16_t arrowW = size / 2;
    int16_t arrowH = size / 2;

    // Arrow head (triangle pointing left)
    tft.fillTriangle(
        cx - arrowW/2, cy,
        cx + arrowW/4, cy - arrowH/2,
        cx + arrowW/4, cy + arrowH/2,
        color
    );

    // Arrow shaft
    tft.fillRect(cx - arrowW/4, cy - 3, arrowW/2 + 2, 6, color);
}

void drawWifiIcon(int16_t cx, int16_t cy, int16_t size, uint16_t color) {
    int16_t baseY = cy + size / 4;

    // Draw three arcs (signal strength indicators)
    for (int i = 0; i < 3; i++) {
        int16_t r = (i + 1) * size / 4;
        for (int a = -45; a <= 45; a += 10) {
            float rad = a * 3.14159 / 180.0;
            int16_t x1 = cx + cos(rad - 3.14159/2) * r;
            int16_t y1 = baseY + sin(rad - 3.14159/2) * r;
            int16_t x2 = cx + cos(rad - 3.14159/2 + 0.17) * r;
            int16_t y2 = baseY + sin(rad - 3.14159/2 + 0.17) * r;
            tft.drawLine(x1, y1, x2, y2, color);
        }
    }

    // Draw center dot
    tft.fillCircle(cx, baseY, 2, color);
}

void drawWifiStatusIndicator() {
    bool connected = isWifiConnected();

    if (connected) {
        drawWifiIcon(WIFI_STATUS_X, WIFI_STATUS_Y, WIFI_STATUS_SIZE, COLOR_CONNECTED);
    } else {
        // Clear the area if not connected
        tft.fillRect(WIFI_STATUS_X - WIFI_STATUS_SIZE/2 - 2,
                     WIFI_STATUS_Y - WIFI_STATUS_SIZE/2 - 2,
                     WIFI_STATUS_SIZE + 4, WIFI_STATUS_SIZE + 4, COLOR_BACKGROUND);
    }
}
