#include "screen_wifi.h"
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <ctype.h>
#include <string.h>

// =============================================================================
// WiFi State (exported for other modules)
// =============================================================================

char wifiSsid[MAX_SSID_LEN + 1] = "";
char wifiPassword[MAX_PASS_LEN + 1] = "";

// =============================================================================
// Local State
// =============================================================================

static int activeInput = 0;  // 0 = none, 1 = SSID, 2 = password
static Preferences prefs;

// WiFi scan results
struct WifiNetwork {
    char ssid[MAX_SSID_LEN + 1];
    int32_t rssi;
};
static WifiNetwork wifiNetworks[MAX_WIFI_NETWORKS];
static int wifiNetworkCount = 0;
static bool wifiScanInProgress = false;

// Button states
static bool wifiBackButtonPressed = false;
static bool wifiModeButtonPressed = false;
static bool wifiScanButtonPressed = false;
static bool lastTouchState = false;

// Keyboard state
static bool keyboardVisible = false;
static bool keyboardShift = false;
static bool keyboardSymbols = false;
static char* keyboardTarget = nullptr;
static int keyboardTargetMax = 0;
static const char* keyboardLabel = "";
static bool keyboardIsPassword = false;

// QWERTY keyboard layouts
static const char* kbRowLetters[] = {
    "qwertyuiop",
    "asdfghjkl",
    "zxcvbnm"
};
static const char* kbRowNumbers = "1234567890";
static const char* kbRowSymbols1 = "!@#$%^&*()";
static const char* kbRowSymbols2 = "-_=+[]{}";
static const char* kbRowSymbols3 = ";:'\",.?/";

// =============================================================================
// Helper Functions
// =============================================================================

static const char* getWifiModeString() {
    int mode = getWifiMode();
    switch (mode) {
        case 0: return "Mode: Disabled";
        case 1: return "Mode: Client";
        default: return "Mode: Unknown";
    }
}

static void saveWifiSettings() {
    prefs.begin("wifi", false);
    prefs.putInt("mode", getWifiMode());
    prefs.putString("ssid", wifiSsid);
    prefs.putString("pass", wifiPassword);
    prefs.end();
}

static void connectToWifi() {
    if (getWifiMode() == 1 && strlen(wifiSsid) > 0) {
        WiFi.disconnect();
        WiFi.mode(WIFI_STA);
        WiFi.begin(wifiSsid, wifiPassword);
        Serial.printf("Connecting to WiFi: %s\n", wifiSsid);
    }
}

static void scanWifiNetworks() {
    if (wifiScanInProgress || getWifiMode() == 0) return;

    wifiScanInProgress = true;
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    int n = WiFi.scanNetworks();
    wifiNetworkCount = 0;

    // Sort by signal strength and take top 5
    for (int i = 0; i < n && wifiNetworkCount < MAX_WIFI_NETWORKS; i++) {
        int bestIdx = -1;
        int32_t bestRssi = -999;
        for (int j = 0; j < n; j++) {
            bool alreadyAdded = false;
            for (int k = 0; k < wifiNetworkCount; k++) {
                if (strcmp(wifiNetworks[k].ssid, WiFi.SSID(j).c_str()) == 0) {
                    alreadyAdded = true;
                    break;
                }
            }
            if (!alreadyAdded && WiFi.RSSI(j) > bestRssi) {
                bestRssi = WiFi.RSSI(j);
                bestIdx = j;
            }
        }
        if (bestIdx >= 0) {
            strncpy(wifiNetworks[wifiNetworkCount].ssid, WiFi.SSID(bestIdx).c_str(), MAX_SSID_LEN);
            wifiNetworks[wifiNetworkCount].ssid[MAX_SSID_LEN] = '\0';
            wifiNetworks[wifiNetworkCount].rssi = bestRssi;
            wifiNetworkCount++;
        }
    }

    WiFi.scanDelete();
    wifiScanInProgress = false;
}

// =============================================================================
// Drawing Functions
// =============================================================================

static void drawWifiBackButton(bool pressed) {
    TFT_eSPI& tft = getTft();
    uint16_t btnColor = pressed ? COLOR_BTN_PRESSED : COLOR_BTN_NORMAL;

    tft.fillRoundRect(WIFI_BACK_BTN_X, WIFI_BACK_BTN_Y, WIFI_BACK_BTN_W, WIFI_BACK_BTN_H, 4, btnColor);
    tft.drawRoundRect(WIFI_BACK_BTN_X, WIFI_BACK_BTN_Y, WIFI_BACK_BTN_W, WIFI_BACK_BTN_H, 4, COLOR_BTN_TEXT);

    int16_t cx = WIFI_BACK_BTN_X + 12;
    int16_t cy = WIFI_BACK_BTN_Y + WIFI_BACK_BTN_H / 2;

    tft.fillTriangle(cx - 4, cy, cx + 2, cy - 5, cx + 2, cy + 5, COLOR_BTN_TEXT);

    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_BTN_TEXT, btnColor);
    tft.drawString("Back", cx + 6, cy);
}

static void drawWifiModeButton() {
    TFT_eSPI& tft = getTft();
    uint16_t btnColor = wifiModeButtonPressed ? COLOR_BTN_PRESSED : COLOR_BTN_NORMAL;
    int16_t y = WIFI_CONTENT_Y + WIFI_MODE_BTN_Y;

    tft.fillRoundRect(WIFI_MODE_BTN_X, y, WIFI_MODE_BTN_W, WIFI_MODE_BTN_H, 6, btnColor);
    tft.drawRoundRect(WIFI_MODE_BTN_X, y, WIFI_MODE_BTN_W, WIFI_MODE_BTN_H, 6, COLOR_BTN_TEXT);

    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_BTN_TEXT, btnColor);
    tft.drawString(getWifiModeString(), WIFI_MODE_BTN_X + WIFI_MODE_BTN_W / 2, y + WIFI_MODE_BTN_H / 2);
}

static void drawWifiScanButton(bool pressed) {
    TFT_eSPI& tft = getTft();
    uint16_t btnColor = pressed ? COLOR_BTN_PRESSED : COLOR_BTN_NORMAL;
    int16_t y = WIFI_CONTENT_Y + WIFI_SCAN_BTN_Y;

    tft.fillRoundRect(WIFI_SCAN_BTN_X, y, WIFI_SCAN_BTN_W, WIFI_SCAN_BTN_H, 4, btnColor);
    tft.drawRoundRect(WIFI_SCAN_BTN_X, y, WIFI_SCAN_BTN_W, WIFI_SCAN_BTN_H, 4, COLOR_BTN_TEXT);

    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_BTN_TEXT, btnColor);
    tft.drawString("SCAN", WIFI_SCAN_BTN_X + WIFI_SCAN_BTN_W / 2, y + WIFI_SCAN_BTN_H / 2);
}

static void drawWifiInput(int16_t y, const char* label, const char* value, bool active) {
    TFT_eSPI& tft = getTft();

    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_LABEL, COLOR_BACKGROUND);
    tft.drawString(label, WIFI_INPUT_X, y - 12);

    uint16_t boxColor = active ? COLOR_BTN_PRESSED : COLOR_BTN_NORMAL;
    tft.fillRoundRect(WIFI_INPUT_X, y, WIFI_INPUT_W, WIFI_INPUT_H, 4, boxColor);
    tft.drawRoundRect(WIFI_INPUT_X, y, WIFI_INPUT_W, WIFI_INPUT_H, 4,
                      active ? COLOR_CONNECTED : COLOR_BTN_TEXT);

    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_RPM_TEXT, boxColor);
    tft.drawString(value, WIFI_INPUT_X + 6, y + WIFI_INPUT_H / 2);

    if (active) {
        int textWidth = tft.textWidth(value);
        tft.fillRect(WIFI_INPUT_X + 6 + textWidth + 2, y + 6, 2, WIFI_INPUT_H - 12, COLOR_RPM_TEXT);
    }
}

static void drawWifiNetworkList() {
    TFT_eSPI& tft = getTft();
    int16_t baseY = WIFI_CONTENT_Y + WIFI_LIST_Y;
    int16_t listH = MAX_WIFI_NETWORKS * WIFI_LIST_H;

    tft.fillRect(WIFI_INPUT_X, baseY, WIFI_INPUT_W, listH, COLOR_BACKGROUND);
    tft.drawRect(WIFI_INPUT_X, baseY, WIFI_INPUT_W, listH, COLOR_BTN_TEXT);

    if (wifiNetworkCount == 0) {
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COLOR_BTN_TEXT, COLOR_BACKGROUND);
        const char* msg;
        if (getWifiMode() == 0) {
            msg = "WiFi Disabled";
        } else if (wifiScanInProgress) {
            msg = "Scanning...";
        } else {
            msg = "Press SCAN to search";
        }
        tft.drawString(msg, WIFI_INPUT_X + WIFI_INPUT_W / 2, baseY + listH / 2);
        return;
    }

    for (int i = 0; i < wifiNetworkCount; i++) {
        int16_t itemY = baseY + i * WIFI_LIST_H;

        if (i % 2 == 1) {
            tft.fillRect(WIFI_INPUT_X + 1, itemY, WIFI_INPUT_W - 2, WIFI_LIST_H, COLOR_KB_BG);
        }

        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(COLOR_RPM_TEXT, COLOR_BACKGROUND);
        tft.drawString(wifiNetworks[i].ssid, WIFI_INPUT_X + 6, itemY + WIFI_LIST_H / 2);

        // Signal strength bars
        int bars = 0;
        if (wifiNetworks[i].rssi > -50) bars = 4;
        else if (wifiNetworks[i].rssi > -60) bars = 3;
        else if (wifiNetworks[i].rssi > -70) bars = 2;
        else bars = 1;

        int16_t barX = WIFI_INPUT_X + WIFI_INPUT_W - 30;
        for (int b = 0; b < 4; b++) {
            int16_t barH = 4 + b * 3;
            uint16_t barColor = (b < bars) ? COLOR_CONNECTED : COLOR_BTN_NORMAL;
            tft.fillRect(barX + b * 6, itemY + WIFI_LIST_H - barH - 2, 4, barH, barColor);
        }
    }
}

static void drawWifiScreenContent() {
    TFT_eSPI& tft = getTft();
    int16_t contentEndY = SCREEN_HEIGHT - 50;
    tft.fillRect(0, WIFI_CONTENT_Y, SCREEN_WIDTH, contentEndY - WIFI_CONTENT_Y, COLOR_BACKGROUND);

    drawWifiModeButton();
    drawWifiInput(WIFI_CONTENT_Y + WIFI_SSID_Y, "SSID:", wifiSsid, activeInput == 1);
    drawWifiInput(WIFI_CONTENT_Y + WIFI_PASS_Y, "Password:", wifiPassword, activeInput == 2);

    if (getWifiMode() == 1) {
        drawWifiScanButton(wifiScanButtonPressed);
        drawWifiNetworkList();
    }
}

// =============================================================================
// Keyboard Functions
// =============================================================================

static void drawKeyboardKey(int16_t x, int16_t y, int16_t w, const char* label, uint16_t color) {
    TFT_eSPI& tft = getTft();
    tft.fillRoundRect(x, y, w, KB_KEY_H, 4, color);
    tft.drawRoundRect(x, y, w, KB_KEY_H, 4, COLOR_BTN_TEXT);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_BTN_TEXT, color);
    tft.drawString(label, x + w / 2, y + KB_KEY_H / 2);
}

static void drawKeyboard() {
    if (!keyboardVisible) return;

    TFT_eSPI& tft = getTft();
    tft.fillScreen(COLOR_KB_BG);

    // Header
    tft.fillRect(0, 0, SCREEN_WIDTH, KB_HEADER_H, COLOR_BACKGROUND);
    tft.drawLine(0, KB_HEADER_H, SCREEN_WIDTH, KB_HEADER_H, COLOR_BTN_TEXT);

    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_LABEL, COLOR_BACKGROUND);
    tft.drawString(keyboardLabel, 10, 5);

    // Input box
    tft.fillRoundRect(5, 18, SCREEN_WIDTH - 10, 24, 4, COLOR_BTN_NORMAL);
    tft.drawRoundRect(5, 18, SCREEN_WIDTH - 10, 24, 4, COLOR_CONNECTED);

    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_RPM_TEXT, COLOR_BTN_NORMAL);

    if (keyboardTarget) {
        if (keyboardIsPassword && strlen(keyboardTarget) > 0) {
            char masked[MAX_PASS_LEN + 1];
            int len = strlen(keyboardTarget);
            for (int i = 0; i < len && i < MAX_PASS_LEN; i++) masked[i] = '*';
            masked[len] = '\0';
            tft.drawString(masked, 12, 30);
        } else {
            tft.drawString(keyboardTarget, 12, 30);
        }
    }

    int16_t y = KB_START_Y;
    int16_t x;
    char keyStr[2] = {0, 0};

    // Row 1: Numbers or symbols
    const char* row1 = keyboardSymbols ? kbRowSymbols1 : kbRowNumbers;
    x = 2;
    for (int i = 0; i < 10; i++) {
        keyStr[0] = row1[i];
        drawKeyboardKey(x, y, KB_KEY_W, keyStr, COLOR_BTN_NORMAL);
        x += KB_KEY_W + KB_SPACING;
    }
    y += KB_KEY_H + KB_SPACING;

    // Row 2: QWERTYUIOP or symbols
    const char* row2 = keyboardSymbols ? kbRowSymbols2 : kbRowLetters[0];
    int row2len = strlen(row2);
    x = (SCREEN_WIDTH - (row2len * (KB_KEY_W + KB_SPACING) - KB_SPACING)) / 2;
    for (int i = 0; i < row2len; i++) {
        keyStr[0] = row2[i];
        if (!keyboardSymbols && keyboardShift) keyStr[0] = toupper(keyStr[0]);
        drawKeyboardKey(x, y, KB_KEY_W, keyStr, COLOR_BTN_NORMAL);
        x += KB_KEY_W + KB_SPACING;
    }
    y += KB_KEY_H + KB_SPACING;

    // Row 3: ASDFGHJKL or symbols
    const char* row3 = keyboardSymbols ? kbRowSymbols3 : kbRowLetters[1];
    int row3len = strlen(row3);
    x = (SCREEN_WIDTH - (row3len * (KB_KEY_W + KB_SPACING) - KB_SPACING)) / 2;
    for (int i = 0; i < row3len; i++) {
        keyStr[0] = row3[i];
        if (!keyboardSymbols && keyboardShift) keyStr[0] = toupper(keyStr[0]);
        drawKeyboardKey(x, y, KB_KEY_W, keyStr, COLOR_BTN_NORMAL);
        x += KB_KEY_W + KB_SPACING;
    }
    y += KB_KEY_H + KB_SPACING;

    // Row 4: SHIFT + ZXCVBNM + DEL
    x = 2;
    drawKeyboardKey(x, y, KB_WIDE_KEY_W, "SHIFT", keyboardShift ? COLOR_CONNECTED : COLOR_BTN_NORMAL);
    x += KB_WIDE_KEY_W + KB_SPACING;

    const char* row4 = kbRowLetters[2];
    for (int i = 0; i < 7; i++) {
        keyStr[0] = row4[i];
        if (keyboardShift) keyStr[0] = toupper(keyStr[0]);
        drawKeyboardKey(x, y, KB_KEY_W, keyStr, COLOR_BTN_NORMAL);
        x += KB_KEY_W + KB_SPACING;
    }
    drawKeyboardKey(x, y, KB_WIDE_KEY_W, "DEL", COLOR_DISCONNECTED);
    y += KB_KEY_H + KB_SPACING;

    // Row 5: ?123 + SPACE + . + OK + BACK
    x = 2;
    drawKeyboardKey(x, y, KB_WIDE_KEY_W, keyboardSymbols ? "ABC" : "?123", COLOR_BTN_NORMAL);
    x += KB_WIDE_KEY_W + KB_SPACING;

    drawKeyboardKey(x, y, KB_SPACE_W, "SPACE", COLOR_BTN_NORMAL);
    x += KB_SPACE_W + KB_SPACING;

    drawKeyboardKey(x, y, KB_KEY_W, ".", COLOR_BTN_NORMAL);
    x += KB_KEY_W + KB_SPACING;

    drawKeyboardKey(x, y, KB_WIDE_KEY_W, "OK", COLOR_CONNECTED);
    x += KB_WIDE_KEY_W + KB_SPACING;

    drawKeyboardKey(x, y, KB_WIDE_KEY_W, "BACK", COLOR_WARNING);
}

static void showKeyboard(const char* label, char* target, int maxLen, bool isPassword) {
    keyboardLabel = label;
    keyboardTarget = target;
    keyboardTargetMax = maxLen;
    keyboardIsPassword = isPassword;
    keyboardShift = false;
    keyboardSymbols = false;
    keyboardVisible = true;
    drawKeyboard();
}

static void hideKeyboard(bool save) {
    keyboardVisible = false;
    keyboardTarget = nullptr;
    if (save) {
        saveWifiSettings();
        connectToWifi();
    }
}

static void keyboardAddChar(char c) {
    if (!keyboardTarget) return;
    int len = strlen(keyboardTarget);
    if (len < keyboardTargetMax) {
        keyboardTarget[len] = c;
        keyboardTarget[len + 1] = '\0';
    }
}

static void keyboardDeleteChar() {
    if (!keyboardTarget) return;
    int len = strlen(keyboardTarget);
    if (len > 0) {
        keyboardTarget[len - 1] = '\0';
    }
}

static void handleKeyboardTouch(int16_t touchX, int16_t touchY) {
    if (!keyboardVisible) return;

    int16_t y = KB_START_Y;

    // Row 1: Numbers/symbols
    if (touchY >= y && touchY < y + KB_KEY_H) {
        const char* row1 = keyboardSymbols ? kbRowSymbols1 : kbRowNumbers;
        int col = (touchX - 2) / (KB_KEY_W + KB_SPACING);
        if (col >= 0 && col < 10) {
            keyboardAddChar(row1[col]);
            drawKeyboard();
        }
        return;
    }
    y += KB_KEY_H + KB_SPACING;

    // Row 2
    if (touchY >= y && touchY < y + KB_KEY_H) {
        const char* row2 = keyboardSymbols ? kbRowSymbols2 : kbRowLetters[0];
        int row2len = strlen(row2);
        int16_t startX = (SCREEN_WIDTH - (row2len * (KB_KEY_W + KB_SPACING) - KB_SPACING)) / 2;
        int col = (touchX - startX) / (KB_KEY_W + KB_SPACING);
        if (col >= 0 && col < row2len) {
            char c = row2[col];
            if (!keyboardSymbols && keyboardShift) c = toupper(c);
            keyboardAddChar(c);
            if (keyboardShift) keyboardShift = false;
            drawKeyboard();
        }
        return;
    }
    y += KB_KEY_H + KB_SPACING;

    // Row 3
    if (touchY >= y && touchY < y + KB_KEY_H) {
        const char* row3 = keyboardSymbols ? kbRowSymbols3 : kbRowLetters[1];
        int row3len = strlen(row3);
        int16_t startX = (SCREEN_WIDTH - (row3len * (KB_KEY_W + KB_SPACING) - KB_SPACING)) / 2;
        int col = (touchX - startX) / (KB_KEY_W + KB_SPACING);
        if (col >= 0 && col < row3len) {
            char c = row3[col];
            if (!keyboardSymbols && keyboardShift) c = toupper(c);
            keyboardAddChar(c);
            if (keyboardShift) keyboardShift = false;
            drawKeyboard();
        }
        return;
    }
    y += KB_KEY_H + KB_SPACING;

    // Row 4: SHIFT + ZXCVBNM + DEL
    if (touchY >= y && touchY < y + KB_KEY_H) {
        int16_t x = 2;
        if (touchX >= x && touchX < x + KB_WIDE_KEY_W) {
            keyboardShift = !keyboardShift;
            drawKeyboard();
            return;
        }
        x += KB_WIDE_KEY_W + KB_SPACING;

        for (int i = 0; i < 7; i++) {
            if (touchX >= x && touchX < x + KB_KEY_W) {
                char c = kbRowLetters[2][i];
                if (keyboardShift) c = toupper(c);
                keyboardAddChar(c);
                if (keyboardShift) keyboardShift = false;
                drawKeyboard();
                return;
            }
            x += KB_KEY_W + KB_SPACING;
        }

        if (touchX >= x && touchX < x + KB_WIDE_KEY_W) {
            keyboardDeleteChar();
            drawKeyboard();
            return;
        }
        return;
    }
    y += KB_KEY_H + KB_SPACING;

    // Row 5: ?123 + SPACE + . + OK + BACK
    if (touchY >= y && touchY < y + KB_KEY_H) {
        int16_t x = 2;

        if (touchX >= x && touchX < x + KB_WIDE_KEY_W) {
            keyboardSymbols = !keyboardSymbols;
            drawKeyboard();
            return;
        }
        x += KB_WIDE_KEY_W + KB_SPACING;

        if (touchX >= x && touchX < x + KB_SPACE_W) {
            keyboardAddChar(' ');
            drawKeyboard();
            return;
        }
        x += KB_SPACE_W + KB_SPACING;

        if (touchX >= x && touchX < x + KB_KEY_W) {
            keyboardAddChar('.');
            drawKeyboard();
            return;
        }
        x += KB_KEY_W + KB_SPACING;

        if (touchX >= x && touchX < x + KB_WIDE_KEY_W) {
            hideKeyboard(true);
            switchToScreen(SCREEN_WIFI);
            return;
        }
        x += KB_WIDE_KEY_W + KB_SPACING;

        if (touchX >= x && touchX < x + KB_WIDE_KEY_W) {
            hideKeyboard(false);
            switchToScreen(SCREEN_WIFI);
            return;
        }
    }
}

// =============================================================================
// Touch Hit Testing
// =============================================================================

static bool isTouchInWifiModeButton(int16_t x, int16_t y) {
    int16_t btnY = WIFI_CONTENT_Y + WIFI_MODE_BTN_Y;
    return pointInRect(x, y, WIFI_MODE_BTN_X, btnY, WIFI_MODE_BTN_W, WIFI_MODE_BTN_H);
}

static bool isTouchInSsidInput(int16_t x, int16_t y) {
    int16_t inputY = WIFI_CONTENT_Y + WIFI_SSID_Y;
    return pointInRect(x, y, WIFI_INPUT_X, inputY, WIFI_INPUT_W, WIFI_INPUT_H);
}

static bool isTouchInPassInput(int16_t x, int16_t y) {
    int16_t inputY = WIFI_CONTENT_Y + WIFI_PASS_Y;
    return pointInRect(x, y, WIFI_INPUT_X, inputY, WIFI_INPUT_W, WIFI_INPUT_H);
}

static bool isTouchInWifiScanButton(int16_t x, int16_t y) {
    int16_t btnY = WIFI_CONTENT_Y + WIFI_SCAN_BTN_Y;
    return pointInRect(x, y, WIFI_SCAN_BTN_X, btnY, WIFI_SCAN_BTN_W, WIFI_SCAN_BTN_H);
}

static bool isTouchInWifiNetwork(int16_t x, int16_t y, int* networkIndex) {
    int16_t listY = WIFI_CONTENT_Y + WIFI_LIST_Y;
    if (x >= WIFI_INPUT_X && x <= WIFI_INPUT_X + WIFI_INPUT_W &&
        y >= listY && y < listY + (MAX_WIFI_NETWORKS * WIFI_LIST_H)) {
        *networkIndex = (y - listY) / WIFI_LIST_H;
        return (*networkIndex < wifiNetworkCount);
    }
    return false;
}

// =============================================================================
// Public Interface
// =============================================================================

void screenWifiInit() {
    prefs.begin("wifi", true);
    setWifiMode(prefs.getInt("mode", 0));
    String ssid = prefs.getString("ssid", "");
    String pass = prefs.getString("pass", "");
    strncpy(wifiSsid, ssid.c_str(), MAX_SSID_LEN);
    strncpy(wifiPassword, pass.c_str(), MAX_PASS_LEN);
    prefs.end();

    // Apply WiFi mode
    if (getWifiMode() == 0) {
        WiFi.mode(WIFI_OFF);
    } else if (getWifiMode() == 1) {
        WiFi.mode(WIFI_STA);
        if (strlen(wifiSsid) > 0) {
            WiFi.begin(wifiSsid, wifiPassword);
        }
    }
}

void screenWifiDraw() {
    TFT_eSPI& tft = getTft();
    tft.fillScreen(COLOR_BACKGROUND);

    drawWifiBackButton(false);

    tft.setTextColor(COLOR_LABEL, COLOR_BACKGROUND);
    tft.setTextDatum(TC_DATUM);
    tft.setTextSize(2);
    tft.drawString("WIFI SETTINGS", SCREEN_WIDTH / 2 + 20, 10);

    tft.drawLine(20, 35, SCREEN_WIDTH - 20, 35, COLOR_LABEL);

    drawWifiScreenContent();
    drawWifiStatusIndicator();
}

void screenWifiHandleTouch(int16_t x, int16_t y, bool pressed) {
    if (keyboardVisible) {
        if (pressed && !lastTouchState) {
            handleKeyboardTouch(x, y);
        }
        lastTouchState = pressed;
        return;
    }

    if (pressed) {
        // Back button
        if (pointInRect(x, y, WIFI_BACK_BTN_X, WIFI_BACK_BTN_Y, WIFI_BACK_BTN_W, WIFI_BACK_BTN_H) &&
            !wifiBackButtonPressed) {
            wifiBackButtonPressed = true;
            drawWifiBackButton(true);
            Serial.println("WIFI BACK button pressed");
        }

        // Scan button
        if (getWifiMode() == 1 && isTouchInWifiScanButton(x, y) && !wifiScanButtonPressed) {
            wifiScanButtonPressed = true;
            drawWifiScanButton(true);
            Serial.println("SCAN button pressed");
        }

        // Mode button
        if (isTouchInWifiModeButton(x, y) && !wifiModeButtonPressed) {
            wifiModeButtonPressed = true;
            drawWifiModeButton();
            Serial.println("MODE button pressed");
        }

        // SSID input
        if (isTouchInSsidInput(x, y) && !lastTouchState) {
            activeInput = 1;
            showKeyboard("SSID:", wifiSsid, MAX_SSID_LEN, false);
            Serial.println("SSID input selected");
        }

        // Password input
        if (isTouchInPassInput(x, y) && !lastTouchState) {
            activeInput = 2;
            showKeyboard("Password:", wifiPassword, MAX_PASS_LEN, true);
            Serial.println("Password input selected");
        }

        // Network list selection
        if (getWifiMode() == 1) {
            int networkIdx;
            if (isTouchInWifiNetwork(x, y, &networkIdx) && !lastTouchState) {
                strncpy(wifiSsid, wifiNetworks[networkIdx].ssid, MAX_SSID_LEN);
                wifiSsid[MAX_SSID_LEN] = '\0';
                activeInput = 2;
                showKeyboard("Password:", wifiPassword, MAX_PASS_LEN, true);
                Serial.printf("Selected network: %s\n", wifiSsid);
            }
        }
    } else {
        // Handle releases
        if (wifiBackButtonPressed) {
            wifiBackButtonPressed = false;
            activeInput = 0;
            switchToScreen(SCREEN_MAIN);
            Serial.println("Switching to main screen");
        }

        if (wifiScanButtonPressed) {
            wifiScanButtonPressed = false;
            drawWifiScanButton(false);
            scanWifiNetworks();
            drawWifiNetworkList();
            drawWifiBackButton(false);
            Serial.println("WiFi scan complete");
        }

        if (wifiModeButtonPressed) {
            wifiModeButtonPressed = false;
            int mode = getWifiMode();
            setWifiMode((mode == 0) ? 1 : 0);
            saveWifiSettings();

            if (getWifiMode() == 0) {
                WiFi.disconnect(true);
                WiFi.mode(WIFI_OFF);
                wifiNetworkCount = 0;
            }

            drawWifiScreenContent();
            drawWifiBackButton(false);
            Serial.printf("WiFi mode: %s\n", getWifiModeString());
        }
    }

    lastTouchState = pressed;
}

void screenWifiUpdate() {
    // Nothing to update currently
}

void screenWifiReset() {
    activeInput = 0;
    wifiBackButtonPressed = false;
    wifiModeButtonPressed = false;
    wifiScanButtonPressed = false;
    lastTouchState = false;

    if (keyboardVisible) {
        hideKeyboard(false);
    }
}

bool screenWifiKeyboardVisible() {
    return keyboardVisible;
}
