#include "keyboard.h"
#include "../display_common.h"
#include <Arduino.h>
#include <ctype.h>
#include <string.h>

// =============================================================================
// Keyboard State
// =============================================================================

static bool kbVisible = false;
static bool kbShift = false;
static bool kbSymbols = false;
static char* kbTarget = nullptr;
static int kbTargetMax = 0;
static const char* kbLabel = "";
static bool kbIsPassword = false;
static KeyboardCallback kbCallback = nullptr;

// =============================================================================
// QWERTY Keyboard Layouts
// =============================================================================

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
// Internal Helper Functions
// =============================================================================

static void keyboardAddChar(char c) {
    if (!kbTarget) return;
    int len = strlen(kbTarget);
    if (len < kbTargetMax) {
        kbTarget[len] = c;
        kbTarget[len + 1] = '\0';
    }
}

static void keyboardDeleteChar() {
    if (!kbTarget) return;
    int len = strlen(kbTarget);
    if (len > 0) {
        kbTarget[len - 1] = '\0';
    }
}

static void drawKeyboardKey(int16_t x, int16_t y, int16_t w, const char* label, uint16_t color) {
    TFT_eSPI& tft = getTft();
    tft.fillRoundRect(x, y, w, KB_KEY_H, 4, color);
    tft.drawRoundRect(x, y, w, KB_KEY_H, 4, COLOR_BTN_TEXT);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_BTN_TEXT, color);
    tft.drawString(label, x + w / 2, y + KB_KEY_H / 2);
}

// =============================================================================
// Public Interface Implementation
// =============================================================================

void keyboardShow(const char* label, char* target, int maxLen, bool isPassword, KeyboardCallback callback) {
    kbLabel = label;
    kbTarget = target;
    kbTargetMax = maxLen;
    kbIsPassword = isPassword;
    kbCallback = callback;
    kbShift = false;
    kbSymbols = false;
    kbVisible = true;
    keyboardDraw();
}

void keyboardHide(bool save) {
    kbVisible = false;
    KeyboardCallback cb = kbCallback;
    kbTarget = nullptr;
    kbCallback = nullptr;
    if (cb) {
        cb(save);
    }
}

bool keyboardIsVisible() {
    return kbVisible;
}

char* keyboardGetTarget() {
    return kbTarget;
}

void keyboardDraw() {
    if (!kbVisible) return;

    TFT_eSPI& tft = getTft();
    tft.fillScreen(COLOR_KB_BG);

    // Header
    tft.fillRect(0, 0, SCREEN_WIDTH, KB_HEADER_H, COLOR_BACKGROUND);
    tft.drawLine(0, KB_HEADER_H, SCREEN_WIDTH, KB_HEADER_H, COLOR_BTN_TEXT);

    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_LABEL, COLOR_BACKGROUND);
    tft.drawString(kbLabel, 10, 5);

    // Input box
    tft.fillRoundRect(5, 18, SCREEN_WIDTH - 10, 24, 4, COLOR_BTN_NORMAL);
    tft.drawRoundRect(5, 18, SCREEN_WIDTH - 10, 24, 4, COLOR_CONNECTED);

    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_RPM_TEXT, COLOR_BTN_NORMAL);

    if (kbTarget) {
        if (kbIsPassword && strlen(kbTarget) > 0) {
            // Show masked password
            char masked[128];
            int len = strlen(kbTarget);
            for (int i = 0; i < len && i < 127; i++) masked[i] = '*';
            masked[len < 127 ? len : 127] = '\0';
            tft.drawString(masked, 12, 30);
        } else {
            tft.drawString(kbTarget, 12, 30);
        }
    }

    int16_t y = KB_START_Y;
    int16_t x;
    char keyStr[2] = {0, 0};

    // Row 1: Numbers or symbols
    const char* row1 = kbSymbols ? kbRowSymbols1 : kbRowNumbers;
    x = 2;
    for (int i = 0; i < 10; i++) {
        keyStr[0] = row1[i];
        drawKeyboardKey(x, y, KB_KEY_W, keyStr, COLOR_BTN_NORMAL);
        x += KB_KEY_W + KB_SPACING;
    }
    y += KB_KEY_H + KB_SPACING;

    // Row 2: QWERTYUIOP or symbols
    const char* row2 = kbSymbols ? kbRowSymbols2 : kbRowLetters[0];
    int row2len = strlen(row2);
    x = (SCREEN_WIDTH - (row2len * (KB_KEY_W + KB_SPACING) - KB_SPACING)) / 2;
    for (int i = 0; i < row2len; i++) {
        keyStr[0] = row2[i];
        if (!kbSymbols && kbShift) keyStr[0] = toupper(keyStr[0]);
        drawKeyboardKey(x, y, KB_KEY_W, keyStr, COLOR_BTN_NORMAL);
        x += KB_KEY_W + KB_SPACING;
    }
    y += KB_KEY_H + KB_SPACING;

    // Row 3: ASDFGHJKL or symbols
    const char* row3 = kbSymbols ? kbRowSymbols3 : kbRowLetters[1];
    int row3len = strlen(row3);
    x = (SCREEN_WIDTH - (row3len * (KB_KEY_W + KB_SPACING) - KB_SPACING)) / 2;
    for (int i = 0; i < row3len; i++) {
        keyStr[0] = row3[i];
        if (!kbSymbols && kbShift) keyStr[0] = toupper(keyStr[0]);
        drawKeyboardKey(x, y, KB_KEY_W, keyStr, COLOR_BTN_NORMAL);
        x += KB_KEY_W + KB_SPACING;
    }
    y += KB_KEY_H + KB_SPACING;

    // Row 4: SHIFT + ZXCVBNM + DEL
    x = 2;
    drawKeyboardKey(x, y, KB_WIDE_KEY_W, "SHIFT", kbShift ? COLOR_CONNECTED : COLOR_BTN_NORMAL);
    x += KB_WIDE_KEY_W + KB_SPACING;

    const char* row4 = kbRowLetters[2];
    for (int i = 0; i < 7; i++) {
        keyStr[0] = row4[i];
        if (kbShift) keyStr[0] = toupper(keyStr[0]);
        drawKeyboardKey(x, y, KB_KEY_W, keyStr, COLOR_BTN_NORMAL);
        x += KB_KEY_W + KB_SPACING;
    }
    drawKeyboardKey(x, y, KB_WIDE_KEY_W, "DEL", COLOR_DISCONNECTED);
    y += KB_KEY_H + KB_SPACING;

    // Row 5: ?123 + SPACE + . + OK + BACK
    x = 2;
    drawKeyboardKey(x, y, KB_WIDE_KEY_W, kbSymbols ? "ABC" : "?123", COLOR_BTN_NORMAL);
    x += KB_WIDE_KEY_W + KB_SPACING;

    drawKeyboardKey(x, y, KB_SPACE_W, "SPACE", COLOR_BTN_NORMAL);
    x += KB_SPACE_W + KB_SPACING;

    drawKeyboardKey(x, y, KB_KEY_W, ".", COLOR_BTN_NORMAL);
    x += KB_KEY_W + KB_SPACING;

    drawKeyboardKey(x, y, KB_WIDE_KEY_W, "OK", COLOR_CONNECTED);
    x += KB_WIDE_KEY_W + KB_SPACING;

    drawKeyboardKey(x, y, KB_WIDE_KEY_W, "BACK", COLOR_WARNING);
}

bool keyboardHandleTouch(int16_t touchX, int16_t touchY) {
    if (!kbVisible) return false;

    int16_t y = KB_START_Y;

    // Row 1: Numbers/symbols
    if (touchY >= y && touchY < y + KB_KEY_H) {
        const char* row1 = kbSymbols ? kbRowSymbols1 : kbRowNumbers;
        int col = (touchX - 2) / (KB_KEY_W + KB_SPACING);
        if (col >= 0 && col < 10) {
            keyboardAddChar(row1[col]);
            keyboardDraw();
        }
        return true;
    }
    y += KB_KEY_H + KB_SPACING;

    // Row 2
    if (touchY >= y && touchY < y + KB_KEY_H) {
        const char* row2 = kbSymbols ? kbRowSymbols2 : kbRowLetters[0];
        int row2len = strlen(row2);
        int16_t startX = (SCREEN_WIDTH - (row2len * (KB_KEY_W + KB_SPACING) - KB_SPACING)) / 2;
        int col = (touchX - startX) / (KB_KEY_W + KB_SPACING);
        if (col >= 0 && col < row2len) {
            char c = row2[col];
            if (!kbSymbols && kbShift) c = toupper(c);
            keyboardAddChar(c);
            if (kbShift) kbShift = false;
            keyboardDraw();
        }
        return true;
    }
    y += KB_KEY_H + KB_SPACING;

    // Row 3
    if (touchY >= y && touchY < y + KB_KEY_H) {
        const char* row3 = kbSymbols ? kbRowSymbols3 : kbRowLetters[1];
        int row3len = strlen(row3);
        int16_t startX = (SCREEN_WIDTH - (row3len * (KB_KEY_W + KB_SPACING) - KB_SPACING)) / 2;
        int col = (touchX - startX) / (KB_KEY_W + KB_SPACING);
        if (col >= 0 && col < row3len) {
            char c = row3[col];
            if (!kbSymbols && kbShift) c = toupper(c);
            keyboardAddChar(c);
            if (kbShift) kbShift = false;
            keyboardDraw();
        }
        return true;
    }
    y += KB_KEY_H + KB_SPACING;

    // Row 4: SHIFT + ZXCVBNM + DEL
    if (touchY >= y && touchY < y + KB_KEY_H) {
        int16_t x = 2;
        if (touchX >= x && touchX < x + KB_WIDE_KEY_W) {
            kbShift = !kbShift;
            keyboardDraw();
            return true;
        }
        x += KB_WIDE_KEY_W + KB_SPACING;

        for (int i = 0; i < 7; i++) {
            if (touchX >= x && touchX < x + KB_KEY_W) {
                char c = kbRowLetters[2][i];
                if (kbShift) c = toupper(c);
                keyboardAddChar(c);
                if (kbShift) kbShift = false;
                keyboardDraw();
                return true;
            }
            x += KB_KEY_W + KB_SPACING;
        }

        if (touchX >= x && touchX < x + KB_WIDE_KEY_W) {
            keyboardDeleteChar();
            keyboardDraw();
            return true;
        }
        return true;
    }
    y += KB_KEY_H + KB_SPACING;

    // Row 5: ?123 + SPACE + . + OK + BACK
    if (touchY >= y && touchY < y + KB_KEY_H) {
        int16_t x = 2;

        if (touchX >= x && touchX < x + KB_WIDE_KEY_W) {
            kbSymbols = !kbSymbols;
            keyboardDraw();
            return true;
        }
        x += KB_WIDE_KEY_W + KB_SPACING;

        if (touchX >= x && touchX < x + KB_SPACE_W) {
            keyboardAddChar(' ');
            keyboardDraw();
            return true;
        }
        x += KB_SPACE_W + KB_SPACING;

        if (touchX >= x && touchX < x + KB_KEY_W) {
            keyboardAddChar('.');
            keyboardDraw();
            return true;
        }
        x += KB_KEY_W + KB_SPACING;

        if (touchX >= x && touchX < x + KB_WIDE_KEY_W) {
            // OK pressed
            keyboardHide(true);
            return true;
        }
        x += KB_WIDE_KEY_W + KB_SPACING;

        if (touchX >= x && touchX < x + KB_WIDE_KEY_W) {
            // BACK pressed
            keyboardHide(false);
            return true;
        }
    }

    return true; // Consume all touches when keyboard is visible
}
