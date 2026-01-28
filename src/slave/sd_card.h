#ifndef SD_CARD_H
#define SD_CARD_H

#include <stdint.h>
#include <stddef.h>

// Initialize SD card
bool sdCardInit();

// Check if SD card is present and mounted
bool sdCardPresent();

// Get SD card info
uint64_t sdCardTotalBytes();
uint64_t sdCardUsedBytes();
const char* sdCardType();

// Read file contents (caller must free returned buffer)
char* sdCardReadFile(const char* path, size_t* length);

// Write data to file (creates or overwrites)
bool sdCardWriteFile(const char* path, const uint8_t* data, size_t length);

// Append data to file
bool sdCardAppendFile(const char* path, const uint8_t* data, size_t length);

// Check if file exists
bool sdCardFileExists(const char* path);

// Delete file
bool sdCardDeleteFile(const char* path);

// List files in directory (for diagnostics)
void sdCardListDir(const char* dirname, uint8_t levels);

#endif // SD_CARD_H
