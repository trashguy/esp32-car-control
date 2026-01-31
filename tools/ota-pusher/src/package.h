#ifndef PACKAGE_H
#define PACKAGE_H

#include <string>
#include <vector>
#include <cstdint>

// =============================================================================
// Package Information
// =============================================================================

struct PackageInfo {
    std::string version;
    std::string created;
    uint32_t displaySize;
    uint32_t controllerSize;
    std::string displayMd5;
    std::string controllerMd5;
    bool valid;
};

// =============================================================================
// Package Format
// =============================================================================
// The package is a simple binary format:
// [4 bytes: manifest size][manifest.json]
// [4 bytes: display.bin size][display.bin]
// [4 bytes: controller.bin size][controller.bin]

// =============================================================================
// Package Functions
// =============================================================================

// Create an OTA package from firmware binaries
// Returns the package as a byte vector
// version: Version string (e.g., "1.2.0" or git tag)
// displayFirmwarePath: Path to display MCU firmware binary
// controllerFirmwarePath: Path to controller MCU firmware binary
std::vector<uint8_t> packageCreate(
    const std::string& version,
    const std::string& displayFirmwarePath,
    const std::string& controllerFirmwarePath
);

// Create and write package to file
bool packageCreateFile(
    const std::string& outputPath,
    const std::string& version,
    const std::string& displayFirmwarePath,
    const std::string& controllerFirmwarePath
);

// Validate a package and extract info
bool packageValidate(const std::vector<uint8_t>& packageData, PackageInfo& outInfo);

// Validate a package file
bool packageValidateFile(const std::string& packagePath, PackageInfo& outInfo);

// Calculate MD5 hash of data
std::string calculateMd5(const std::vector<uint8_t>& data);

// Calculate MD5 hash of file
std::string calculateMd5File(const std::string& filePath);

// Read entire file into byte vector
std::vector<uint8_t> readFile(const std::string& path);

// Get current ISO 8601 timestamp
std::string getTimestamp();

#endif // PACKAGE_H
