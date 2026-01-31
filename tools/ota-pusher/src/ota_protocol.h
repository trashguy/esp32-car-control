#ifndef OTA_PROTOCOL_H
#define OTA_PROTOCOL_H

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

// =============================================================================
// OTA Protocol Constants
// =============================================================================

constexpr uint16_t OTA_PORT_PACKAGE = 3233;
constexpr uint32_t OTA_MAGIC = 0x4F544155;  // "OTAU" in little endian
constexpr uint32_t OTA_PROTOCOL_VERSION = 1;

// =============================================================================
// OTA Protocol Header (sent before package data)
// =============================================================================

struct OtaPacketHeader {
    uint32_t magic;             // OTA_MAGIC
    uint32_t version;           // Protocol version
    uint32_t packageSize;       // Total size of package data
    uint32_t reserved;          // For future use
};

// =============================================================================
// OTA Transfer Result
// =============================================================================

enum class OtaResult {
    Success,
    ConnectionFailed,
    ConnectionTimeout,
    TransferFailed,
    Rejected,
    InvalidResponse
};

std::string otaResultToString(OtaResult result);

// =============================================================================
// Progress Callback
// =============================================================================

// Called during transfer with bytes sent and total bytes
using OtaProgressCallback = std::function<void(size_t sent, size_t total)>;

// =============================================================================
// OTA Protocol Functions
// =============================================================================

// Send an OTA package to a device
// Returns result of the transfer
OtaResult otaSendPackage(
    const std::string& host,
    uint16_t port,
    const std::vector<uint8_t>& packageData,
    OtaProgressCallback progressCallback = nullptr,
    int timeoutSeconds = 60
);

// Send an OTA package from file
OtaResult otaSendPackageFile(
    const std::string& host,
    uint16_t port,
    const std::string& packagePath,
    OtaProgressCallback progressCallback = nullptr,
    int timeoutSeconds = 60
);

#endif // OTA_PROTOCOL_H
