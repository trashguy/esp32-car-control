#ifndef SD_HANDLER_H
#define SD_HANDLER_H

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

// Initialize SD card on dedicated SPI bus
// Returns true if SD card is present and initialized
bool sdInit();

// Check if SD card is mounted and ready
bool sdIsReady();

// Check if a card is physically present (uses card detect pin if available)
bool sdIsCardPresent();

// Get SD card info
uint64_t sdGetTotalBytes();
uint64_t sdGetUsedBytes();
uint64_t sdGetFreeBytes();
const char* sdGetCardType();

// File operations - returns true on success

// Check if file/directory exists
bool sdExists(const char* path);

// Create directory (and parents if needed)
bool sdMkdir(const char* path);

// Remove file
bool sdRemove(const char* path);

// Remove directory (must be empty)
bool sdRmdir(const char* path);

// Rename/move file or directory
bool sdRename(const char* oldPath, const char* newPath);

// Read entire file into buffer
// Returns bytes read, or -1 on error
// If buffer is nullptr, returns file size without reading
int32_t sdReadFile(const char* path, uint8_t* buffer, size_t bufferSize);

// Read file as string (convenience function)
// Returns empty string on error
// Caller should check sdExists() first if distinction from empty file matters
String sdReadFileString(const char* path);

// Write buffer to file (creates or overwrites)
bool sdWriteFile(const char* path, const uint8_t* data, size_t length);

// Write string to file (creates or overwrites)
bool sdWriteFileString(const char* path, const String& content);

// Append buffer to file (creates if doesn't exist)
bool sdAppendFile(const char* path, const uint8_t* data, size_t length);

// Append string to file (creates if doesn't exist)
bool sdAppendFileString(const char* path, const String& content);

// Get file size in bytes, returns -1 if file doesn't exist
int32_t sdFileSize(const char* path);

// Low-level random access operations (for virtual memory system)
// Read data from file at specific offset
// Returns bytes read, or -1 on error
int32_t sdReadFileAt(const char* path, uint32_t offset, uint8_t* buffer, size_t length);

// Write data to file at specific offset (file must exist, will extend if needed)
// Returns bytes written, or -1 on error
int32_t sdWriteFileAt(const char* path, uint32_t offset, const uint8_t* data, size_t length);

// Create a file of specified size, filled with zeros
// Useful for pre-allocating virtual memory swap file
bool sdCreateSparseFile(const char* path, uint32_t size);

// List directory contents
// Callback receives filename (not full path), isDirectory flag
// Return false from callback to stop iteration
typedef bool (*SdListCallback)(const char* name, bool isDirectory, size_t size, void* userData);
bool sdListDir(const char* path, SdListCallback callback, void* userData = nullptr);

// Print directory listing to Serial (for debugging)
void sdPrintDir(const char* path, uint8_t depth = 0);

// Unmount SD card (call before physically removing)
void sdUnmount();

// Attempt to remount SD card (after card swap)
bool sdRemount();

#endif // SD_HANDLER_H
