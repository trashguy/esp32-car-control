#ifndef SHARED_PROTOCOL_H
#define SHARED_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

// I2C Protocol Constants
#define RPM_PACKET_HEADER 0xAA
#define RPM_PACKET_SIZE 4

// RPM Packet Structure (4 bytes)
// Byte 0: Header (0xAA)
// Byte 1: RPM low byte
// Byte 2: RPM high byte
// Byte 3: Checksum (XOR of bytes 0-2)
struct RpmPacket {
    uint8_t header;
    uint16_t rpm;
    uint8_t checksum;
} __attribute__((packed));

// Calculate checksum for RPM packet
inline uint8_t calculateChecksum(uint8_t header, uint16_t rpm) {
    uint8_t rpmLow = rpm & 0xFF;
    uint8_t rpmHigh = (rpm >> 8) & 0xFF;
    return header ^ rpmLow ^ rpmHigh;
}

// Validate received packet
inline bool validatePacket(const uint8_t* data, size_t len) {
    if (len != RPM_PACKET_SIZE) return false;
    if (data[0] != RPM_PACKET_HEADER) return false;

    uint16_t rpm = data[1] | (data[2] << 8);
    uint8_t expectedChecksum = calculateChecksum(data[0], rpm);
    return data[3] == expectedChecksum;
}

// Extract RPM from validated packet
inline uint16_t extractRpm(const uint8_t* data) {
    return data[1] | (data[2] << 8);
}

// Pack RPM into buffer
inline void packRpmPacket(uint8_t* buffer, uint16_t rpm) {
    buffer[0] = RPM_PACKET_HEADER;
    buffer[1] = rpm & 0xFF;
    buffer[2] = (rpm >> 8) & 0xFF;
    buffer[3] = calculateChecksum(buffer[0], rpm);
}

#endif // SHARED_PROTOCOL_H
