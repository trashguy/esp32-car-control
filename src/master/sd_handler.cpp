#include "master/sd_handler.h"
#include "shared/config.h"
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>

// Use FSPI (SPI3) for SD card - separate from HSPI used for slave communication
static SPIClass* sdSpi = nullptr;
static bool sdMounted = false;

bool sdInit() {
    // Check card detect pin if configured
    #ifdef SD_SPI_CD_PIN
    pinMode(SD_SPI_CD_PIN, INPUT_PULLUP);
    if (digitalRead(SD_SPI_CD_PIN) == HIGH) {
        Serial.println("SD: No card detected (CD pin HIGH)");
        return false;
    }
    #endif

    // Initialize FSPI (SPI3) on custom pins for SD card
    sdSpi = new SPIClass(FSPI);
    sdSpi->begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);

    // Initialize SD library with custom SPI and CS pin
    if (!SD.begin(SD_SPI_CS_PIN, *sdSpi, SD_SPI_FREQUENCY)) {
        Serial.println("SD: Mount failed");
        delete sdSpi;
        sdSpi = nullptr;
        return false;
    }

    sdMounted = true;

    // Print card info
    uint8_t cardType = SD.cardType();
    const char* typeStr = "UNKNOWN";
    switch (cardType) {
        case CARD_MMC:  typeStr = "MMC"; break;
        case CARD_SD:   typeStr = "SD"; break;
        case CARD_SDHC: typeStr = "SDHC"; break;
        default: break;
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    uint64_t totalBytes = SD.totalBytes() / (1024 * 1024);
    uint64_t usedBytes = SD.usedBytes() / (1024 * 1024);

    Serial.printf("SD: Mounted %s card, %lluMB total, %lluMB used, %lluMB free\n",
                  typeStr, totalBytes, usedBytes, totalBytes - usedBytes);
    Serial.printf("SD: SPI pins SCK=%d, MISO=%d, MOSI=%d, CS=%d @ %dHz\n",
                  SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN, SD_SPI_FREQUENCY);

    return true;
}

bool sdIsReady() {
    return sdMounted;
}

bool sdIsCardPresent() {
    #ifdef SD_SPI_CD_PIN
    return digitalRead(SD_SPI_CD_PIN) == LOW;
    #else
    return sdMounted;
    #endif
}

uint64_t sdGetTotalBytes() {
    if (!sdMounted) return 0;
    return SD.totalBytes();
}

uint64_t sdGetUsedBytes() {
    if (!sdMounted) return 0;
    return SD.usedBytes();
}

uint64_t sdGetFreeBytes() {
    if (!sdMounted) return 0;
    return SD.totalBytes() - SD.usedBytes();
}

const char* sdGetCardType() {
    if (!sdMounted) return "NONE";
    switch (SD.cardType()) {
        case CARD_MMC:  return "MMC";
        case CARD_SD:   return "SD";
        case CARD_SDHC: return "SDHC";
        default:        return "UNKNOWN";
    }
}

bool sdExists(const char* path) {
    if (!sdMounted) return false;
    return SD.exists(path);
}

bool sdMkdir(const char* path) {
    if (!sdMounted) return false;
    return SD.mkdir(path);
}

bool sdRemove(const char* path) {
    if (!sdMounted) return false;
    return SD.remove(path);
}

bool sdRmdir(const char* path) {
    if (!sdMounted) return false;
    return SD.rmdir(path);
}

bool sdRename(const char* oldPath, const char* newPath) {
    if (!sdMounted) return false;
    return SD.rename(oldPath, newPath);
}

int32_t sdReadFile(const char* path, uint8_t* buffer, size_t bufferSize) {
    if (!sdMounted) return -1;

    File file = SD.open(path, FILE_READ);
    if (!file) {
        return -1;
    }

    size_t fileSize = file.size();

    // If buffer is null, just return file size
    if (buffer == nullptr) {
        file.close();
        return (int32_t)fileSize;
    }

    // Read up to buffer size
    size_t toRead = (fileSize < bufferSize) ? fileSize : bufferSize;
    size_t bytesRead = file.read(buffer, toRead);
    file.close();

    return (int32_t)bytesRead;
}

String sdReadFileString(const char* path) {
    if (!sdMounted) return String();

    File file = SD.open(path, FILE_READ);
    if (!file) {
        return String();
    }

    String content = file.readString();
    file.close();
    return content;
}

bool sdWriteFile(const char* path, const uint8_t* data, size_t length) {
    if (!sdMounted) return false;

    File file = SD.open(path, FILE_WRITE);
    if (!file) {
        Serial.printf("SD: Failed to open %s for writing\n", path);
        return false;
    }

    size_t written = file.write(data, length);
    file.close();

    if (written != length) {
        Serial.printf("SD: Write incomplete %zu/%zu bytes to %s\n", written, length, path);
        return false;
    }

    return true;
}

bool sdWriteFileString(const char* path, const String& content) {
    return sdWriteFile(path, (const uint8_t*)content.c_str(), content.length());
}

bool sdAppendFile(const char* path, const uint8_t* data, size_t length) {
    if (!sdMounted) return false;

    File file = SD.open(path, FILE_APPEND);
    if (!file) {
        Serial.printf("SD: Failed to open %s for appending\n", path);
        return false;
    }

    size_t written = file.write(data, length);
    file.close();

    if (written != length) {
        Serial.printf("SD: Append incomplete %zu/%zu bytes to %s\n", written, length, path);
        return false;
    }

    return true;
}

bool sdAppendFileString(const char* path, const String& content) {
    return sdAppendFile(path, (const uint8_t*)content.c_str(), content.length());
}

int32_t sdFileSize(const char* path) {
    if (!sdMounted) return -1;

    File file = SD.open(path, FILE_READ);
    if (!file) {
        return -1;
    }

    int32_t size = (int32_t)file.size();
    file.close();
    return size;
}

int32_t sdReadFileAt(const char* path, uint32_t offset, uint8_t* buffer, size_t length) {
    if (!sdMounted || buffer == nullptr) return -1;

    File file = SD.open(path, FILE_READ);
    if (!file) {
        return -1;
    }

    if (offset > 0 && !file.seek(offset)) {
        file.close();
        return -1;
    }

    size_t bytesRead = file.read(buffer, length);
    file.close();

    return (int32_t)bytesRead;
}

int32_t sdWriteFileAt(const char* path, uint32_t offset, const uint8_t* data, size_t length) {
    if (!sdMounted || data == nullptr) return -1;

    // Use FILE_WRITE which opens for read/write, creating if needed
    // Note: On FAT32, "r+" mode isn't directly supported, so we use a workaround
    File file = SD.open(path, "r+");
    if (!file) {
        // File doesn't exist, create it
        file = SD.open(path, FILE_WRITE);
        if (!file) {
            return -1;
        }
    }

    if (offset > 0 && !file.seek(offset)) {
        file.close();
        return -1;
    }

    size_t bytesWritten = file.write(data, length);
    file.close();

    return (int32_t)bytesWritten;
}

bool sdCreateSparseFile(const char* path, uint32_t size) {
    if (!sdMounted) return false;

    // Remove existing file
    if (SD.exists(path)) {
        SD.remove(path);
    }

    File file = SD.open(path, FILE_WRITE);
    if (!file) {
        return false;
    }

    // Write in chunks to avoid memory issues
    const size_t chunkSize = 4096;
    uint8_t* zeros = (uint8_t*)calloc(chunkSize, 1);
    if (!zeros) {
        file.close();
        return false;
    }

    uint32_t remaining = size;
    bool success = true;

    while (remaining > 0) {
        size_t toWrite = (remaining > chunkSize) ? chunkSize : remaining;
        if (file.write(zeros, toWrite) != toWrite) {
            success = false;
            break;
        }
        remaining -= toWrite;
    }

    free(zeros);
    file.close();
    return success;
}

bool sdListDir(const char* path, SdListCallback callback, void* userData) {
    if (!sdMounted) return false;

    File root = SD.open(path);
    if (!root || !root.isDirectory()) {
        return false;
    }

    File entry;
    while ((entry = root.openNextFile())) {
        const char* name = entry.name();
        // Skip path prefix if present
        const char* basename = strrchr(name, '/');
        if (basename) {
            basename++;  // Skip the '/'
        } else {
            basename = name;
        }

        bool isDir = entry.isDirectory();
        size_t size = isDir ? 0 : entry.size();

        bool continueIteration = callback(basename, isDir, size, userData);
        entry.close();

        if (!continueIteration) {
            break;
        }
    }

    root.close();
    return true;
}

static void printDirRecursive(const char* path, uint8_t depth, uint8_t maxDepth) {
    if (depth > maxDepth) return;

    File root = SD.open(path);
    if (!root || !root.isDirectory()) {
        return;
    }

    File entry;
    while ((entry = root.openNextFile())) {
        for (uint8_t i = 0; i < depth; i++) {
            Serial.print("  ");
        }

        const char* name = entry.name();
        const char* basename = strrchr(name, '/');
        if (basename) {
            basename++;
        } else {
            basename = name;
        }

        if (entry.isDirectory()) {
            Serial.printf("[%s]/\n", basename);
            // Build full path for recursion
            String subPath = String(path);
            if (!subPath.endsWith("/")) subPath += "/";
            subPath += basename;
            printDirRecursive(subPath.c_str(), depth + 1, maxDepth);
        } else {
            Serial.printf("%s (%u bytes)\n", basename, entry.size());
        }
        entry.close();
    }

    root.close();
}

void sdPrintDir(const char* path, uint8_t depth) {
    if (!sdMounted) {
        Serial.println("SD: Not mounted");
        return;
    }

    Serial.printf("SD: Contents of %s\n", path);
    printDirRecursive(path, 0, depth);
}

void sdUnmount() {
    if (sdMounted) {
        SD.end();
        sdMounted = false;
        Serial.println("SD: Unmounted");
    }
    if (sdSpi) {
        sdSpi->end();
        delete sdSpi;
        sdSpi = nullptr;
    }
}

bool sdRemount() {
    sdUnmount();
    delay(100);  // Brief delay for card to settle
    return sdInit();
}
