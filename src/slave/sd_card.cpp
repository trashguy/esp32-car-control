#include "sd_card.h"
#include "shared/config.h"
#include <Arduino.h>
#include <SD_MMC.h>

static bool sdInitialized = false;
static uint8_t cardType = CARD_NONE;

bool sdCardInit() {
    Serial.println("Initializing SD card (SDMMC)...");
    Serial.printf("  CLK: GPIO %d\n", SD_MMC_CLK);
    Serial.printf("  CMD: GPIO %d\n", SD_MMC_CMD);
    Serial.printf("  D0:  GPIO %d\n", SD_MMC_D0);
    Serial.printf("  Mode: %s\n", SD_MMC_1BIT_MODE ? "1-bit" : "4-bit");

    // Configure SDMMC pins
    SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);

    // Mount SD card in 1-bit mode for better compatibility
    if (!SD_MMC.begin("/sdcard", SD_MMC_1BIT_MODE)) {
        Serial.println("SD_MMC mount failed - check card insertion and wiring");
        sdInitialized = false;
        return false;
    }

    cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("No SD card attached");
        sdInitialized = false;
        return false;
    }

    sdInitialized = true;
    Serial.printf("SD card initialized (SDMMC): %s\n", sdCardType());
    Serial.printf("SD card size: %llu MB\n", SD_MMC.cardSize() / (1024 * 1024));

    return true;
}

bool sdCardPresent() {
    return sdInitialized && (cardType != CARD_NONE);
}

uint64_t sdCardTotalBytes() {
    if (!sdInitialized) return 0;
    return SD_MMC.totalBytes();
}

uint64_t sdCardUsedBytes() {
    if (!sdInitialized) return 0;
    return SD_MMC.usedBytes();
}

const char* sdCardType() {
    switch (cardType) {
        case CARD_MMC:  return "MMC";
        case CARD_SD:   return "SD";
        case CARD_SDHC: return "SDHC";
        default:        return "Unknown";
    }
}

char* sdCardReadFile(const char* path, size_t* length) {
    if (!sdInitialized) {
        *length = 0;
        return nullptr;
    }

    File file = SD_MMC.open(path, FILE_READ);
    if (!file) {
        Serial.printf("Failed to open file: %s\n", path);
        *length = 0;
        return nullptr;
    }

    size_t fileSize = file.size();
    char* buffer = (char*)malloc(fileSize + 1);
    if (!buffer) {
        Serial.println("Failed to allocate memory for file");
        file.close();
        *length = 0;
        return nullptr;
    }

    size_t bytesRead = file.readBytes(buffer, fileSize);
    buffer[bytesRead] = '\0';
    file.close();

    *length = bytesRead;
    return buffer;
}

bool sdCardWriteFile(const char* path, const uint8_t* data, size_t length) {
    if (!sdInitialized) return false;

    File file = SD_MMC.open(path, FILE_WRITE);
    if (!file) {
        Serial.printf("Failed to open file for writing: %s\n", path);
        return false;
    }

    size_t written = file.write(data, length);
    file.close();

    if (written != length) {
        Serial.printf("Write incomplete: %zu of %zu bytes\n", written, length);
        return false;
    }

    return true;
}

bool sdCardAppendFile(const char* path, const uint8_t* data, size_t length) {
    if (!sdInitialized) return false;

    File file = SD_MMC.open(path, FILE_APPEND);
    if (!file) {
        Serial.printf("Failed to open file for appending: %s\n", path);
        return false;
    }

    size_t written = file.write(data, length);
    file.close();

    return (written == length);
}

bool sdCardFileExists(const char* path) {
    if (!sdInitialized) return false;
    return SD_MMC.exists(path);
}

bool sdCardDeleteFile(const char* path) {
    if (!sdInitialized) return false;
    return SD_MMC.remove(path);
}

void sdCardListDir(const char* dirname, uint8_t levels) {
    if (!sdInitialized) {
        Serial.println("SD card not initialized");
        return;
    }

    Serial.printf("Listing directory: %s\n", dirname);

    File root = SD_MMC.open(dirname);
    if (!root) {
        Serial.println("Failed to open directory");
        return;
    }
    if (!root.isDirectory()) {
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            Serial.printf("  DIR : %s\n", file.name());
            if (levels > 0) {
                sdCardListDir(file.path(), levels - 1);
            }
        } else {
            Serial.printf("  FILE: %s  SIZE: %zu\n", file.name(), file.size());
        }
        file = root.openNextFile();
    }
}
