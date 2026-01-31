#ifndef SHARED_OTA_PROTOCOL_H
#define SHARED_OTA_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// =============================================================================
// SPI OTA Protocol for Master Firmware Updates
// =============================================================================
//
// The slave (display MCU) receives OTA packages via WiFi and stores them on
// the SD card. The master (controller MCU) periodically polls the slave for
// OTA status and downloads firmware over SPI when available.
//
// Flow:
// 1. Slave receives package via WiFi, extracts controller.bin to SD
// 2. Master polls slave with OTA_CMD_STATUS during normal operation
// 3. Slave responds with OTA_STATUS_FW_READY when firmware is available
// 4. Master sends OTA_CMD_GET_INFO to get firmware size
// 5. Master sends OTA_CMD_GET_CHUNK requests to download firmware
// 6. Master verifies checksum and flashes itself
// 7. Master sends OTA_CMD_DONE to signal completion, slave cleans up
//
// =============================================================================

// Feature flag: Enable OTA test/verification mode
// Set to 1 to enable VERIFY button and test protocol, 0 to disable
#define OTA_ENABLE_TEST_MODE 0

// OTA packet header (replaces normal 0xAA header during OTA)
#define OTA_PACKET_HEADER 0xBB

// Standard OTA packet size (same as normal SPI packet for compatibility)
#define OTA_PACKET_SIZE 5

// Bulk data packet size (larger packets for firmware transfer)
// Format: header(1) + status(1) + len(2) + data(256) + crc(4) = 264 bytes
#define OTA_BULK_PACKET_SIZE 264

// =============================================================================
// OTA Commands (Master -> Slave in byte 1)
// =============================================================================

#define OTA_CMD_STATUS      0x01  // Query OTA status (normal polling)
#define OTA_CMD_GET_INFO    0x02  // Get firmware info (size, checksum)
#define OTA_CMD_START_BULK  0x03  // Signal master is ready for bulk mode
#define OTA_CMD_GET_CHUNK   0x10  // Request firmware chunk (bulk mode only)
#define OTA_CMD_DONE        0x04  // Firmware received successfully
#define OTA_CMD_ABORT       0x05  // Abort OTA process

// Test commands (for verifying OTA protocol without flashing)
#define OTA_CMD_TEST_START  0x20  // Start OTA test mode
#define OTA_CMD_TEST_CHUNK  0x21  // Request test data chunk
#define OTA_CMD_TEST_END    0x22  // End test mode, return to normal SPI

// Test mode parameters
#define OTA_TEST_FIRMWARE_SIZE  (16 * 1024)  // 16KB of test data
#define OTA_TEST_NUM_CHUNKS     ((OTA_TEST_FIRMWARE_SIZE + OTA_CHUNK_SIZE - 1) / OTA_CHUNK_SIZE)

// =============================================================================
// OTA Status (Slave -> Master responses)
// =============================================================================

#define OTA_STATUS_IDLE             0x00  // No OTA pending
#define OTA_STATUS_FW_READY         0x01  // Controller firmware ready for download
#define OTA_STATUS_BUSY             0x02  // Slave is busy (receiving package, etc)
#define OTA_STATUS_TEST_READY       0x10  // Test mode active, ready for test chunks
#define OTA_STATUS_VERIFY_REQUESTED 0x11  // User requested SPI verification test
#define OTA_STATUS_VERIFY_PASSED    0x12  // Verification test passed
#define OTA_STATUS_VERIFY_FAILED    0x13  // Verification test failed
#define OTA_STATUS_ERROR            0xFF  // Error occurred

// =============================================================================
// OTA Packet Structures
// =============================================================================

// Standard OTA status query packet (5 bytes, compatible with normal SPI)
// Master -> Slave:
//   [0] = OTA_PACKET_HEADER (0xBB)
//   [1] = OTA_CMD_*
//   [2] = param_low (command-specific)
//   [3] = param_high (command-specific)
//   [4] = checksum
//
// Slave -> Master:
//   [0] = OTA_PACKET_HEADER (0xBB)
//   [1] = OTA_STATUS_*
//   [2] = data_low (response-specific)
//   [3] = data_high (response-specific)
//   [4] = checksum

// Firmware info response (after OTA_CMD_GET_INFO)
struct OtaFirmwareInfo {
    uint32_t size;           // Firmware size in bytes
    uint32_t checksum;       // CRC32 of firmware
} __attribute__((packed));

// Bulk transfer packet for firmware chunks
// Request (Master -> Slave):
//   [0] = OTA_PACKET_HEADER
//   [1] = OTA_CMD_GET_CHUNK
//   [2-3] = chunk_index (little endian)
//   [4] = checksum
//
// Response (Slave -> Master, OTA_BULK_PACKET_SIZE bytes):
//   [0] = OTA_PACKET_HEADER
//   [1] = status (0x00 = OK, 0xFF = error)
//   [2-3] = bytes_in_chunk (actual data length)
//   [4-259] = chunk data (256 bytes max)
//   Last 4 bytes = CRC32 of chunk data

#define OTA_CHUNK_SIZE 256  // Bytes per chunk

// =============================================================================
// Helper Functions
// =============================================================================

// Calculate checksum for OTA packet (same algorithm as SPI)
inline uint8_t otaCalculateChecksum(const uint8_t* data, size_t len) {
    uint8_t checksum = 0;
    for (size_t i = 0; i < len - 1; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

// Validate OTA packet
inline bool otaValidatePacket(const uint8_t* data) {
    if (data[0] != OTA_PACKET_HEADER) return false;
    uint8_t expected = otaCalculateChecksum(data, OTA_PACKET_SIZE);
    return data[OTA_PACKET_SIZE - 1] == expected;
}

// Pack OTA command packet
inline void otaPackCommand(uint8_t* buffer, uint8_t cmd, uint16_t param) {
    buffer[0] = OTA_PACKET_HEADER;
    buffer[1] = cmd;
    buffer[2] = param & 0xFF;
    buffer[3] = (param >> 8) & 0xFF;
    buffer[4] = otaCalculateChecksum(buffer, OTA_PACKET_SIZE);
}

// Pack OTA response packet
inline void otaPackResponse(uint8_t* buffer, uint8_t status, uint16_t data) {
    buffer[0] = OTA_PACKET_HEADER;
    buffer[1] = status;
    buffer[2] = data & 0xFF;
    buffer[3] = (data >> 8) & 0xFF;
    buffer[4] = otaCalculateChecksum(buffer, OTA_PACKET_SIZE);
}

// Extract param/data from packet
inline uint16_t otaExtractParam(const uint8_t* data) {
    return data[2] | (data[3] << 8);
}

// =============================================================================
// CRC32 for firmware verification
// =============================================================================

// Simple CRC32 implementation (IEEE 802.3 polynomial)
inline uint32_t otaCrc32(const uint8_t* data, size_t len, uint32_t crc = 0xFFFFFFFF) {
    static const uint32_t table[16] = {
        0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC,
        0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
        0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
        0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C
    };
    
    for (size_t i = 0; i < len; i++) {
        crc = table[(crc ^ data[i]) & 0x0F] ^ (crc >> 4);
        crc = table[(crc ^ (data[i] >> 4)) & 0x0F] ^ (crc >> 4);
    }
    
    return ~crc;
}

#endif // SHARED_OTA_PROTOCOL_H
