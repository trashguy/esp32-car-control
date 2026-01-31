#ifndef SHARED_PROTOCOL_H
#define SHARED_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

// SPI Protocol Constants
// Full-duplex: Master and Slave exchange data simultaneously
#define SPI_PACKET_HEADER 0xAA
#define SPI_PACKET_SIZE 8  // Header + 2 bytes RPM + 1 byte mode + 2 bytes water temp + 1 byte status + checksum

// Mode values for slave->master communication
#define MODE_AUTO   0x00   // Auto mode (display received RPM)
#define MODE_MANUAL 0x01   // Manual mode (display user-set RPM)

// Water temperature sensor status values
#define WATER_TEMP_STATUS_OK          0x00  // Sensor reading valid
#define WATER_TEMP_STATUS_DISCONNECTED 0x01  // Sensor open circuit (disconnected)
#define WATER_TEMP_STATUS_SHORTED     0x02  // Sensor short circuit
#define WATER_TEMP_STATUS_DISABLED    0x03  // Sensor disabled by user

// Special value for invalid temperature reading
#define WATER_TEMP_INVALID  0x7FFF  // Max int16_t value indicates invalid

// Master->Slave Packet Structure (8 bytes)
// Byte 0: Header (0xAA)
// Byte 1: RPM low byte (what slave should display)
// Byte 2: RPM high byte
// Byte 3: Mode (MODE_AUTO or MODE_MANUAL) - master is authoritative
// Byte 4: Water Temp low byte (Fahrenheit * 10, signed int16_t)
// Byte 5: Water Temp high byte
// Byte 6: Water Temp Status (WATER_TEMP_STATUS_*)
// Byte 7: Checksum (XOR of bytes 0-6)

// Slave->Master Packet Structure (8 bytes, sent simultaneously)
// Byte 0: Header (0xAA)
// Byte 1: Requested RPM low byte (slave UI input)
// Byte 2: Requested RPM high byte
// Byte 3: Requested Mode (slave UI input)
// Byte 4-6: Reserved (for future slave->master data)
// Byte 7: Checksum (XOR of bytes 0-6)

struct SpiPacket {
    uint8_t header;
    uint16_t value;      // RPM (master->slave) or Manual RPM (slave->master)
    uint8_t aux;         // Mode
    int16_t waterTempF10; // Water temp in Fahrenheit * 10 (master->slave) or reserved
    uint8_t waterStatus; // Water temp status (master->slave) or reserved
    uint8_t checksum;
} __attribute__((packed));

// Calculate checksum for SPI packet (XOR of bytes 0-6)
inline uint8_t calculateSpiChecksum(const uint8_t* data) {
    return data[0] ^ data[1] ^ data[2] ^ data[3] ^ data[4] ^ data[5] ^ data[6];
}

// Validate received packet
inline bool validateSpiPacket(const uint8_t* data) {
    if (data[0] != SPI_PACKET_HEADER) return false;
    uint8_t expectedChecksum = calculateSpiChecksum(data);
    return data[7] == expectedChecksum;
}

// Extract RPM from validated packet
inline uint16_t extractSpiRpm(const uint8_t* data) {
    return data[1] | (data[2] << 8);
}

// Extract mode from validated packet
inline uint8_t extractSpiMode(const uint8_t* data) {
    return data[3];
}

// Extract water temperature from validated master packet (Fahrenheit * 10)
inline int16_t extractSpiWaterTempF10(const uint8_t* data) {
    return (int16_t)(data[4] | (data[5] << 8));
}

// Extract water temperature status from validated master packet
inline uint8_t extractSpiWaterTempStatus(const uint8_t* data) {
    return data[6];
}

// Pack master->slave packet (RPM to display + authoritative mode + water temp)
inline void packMasterPacket(uint8_t* buffer, uint16_t rpm, uint8_t mode, 
                              int16_t waterTempF10, uint8_t waterStatus) {
    buffer[0] = SPI_PACKET_HEADER;
    buffer[1] = rpm & 0xFF;
    buffer[2] = (rpm >> 8) & 0xFF;
    buffer[3] = mode;
    buffer[4] = waterTempF10 & 0xFF;
    buffer[5] = (waterTempF10 >> 8) & 0xFF;
    buffer[6] = waterStatus;
    buffer[7] = calculateSpiChecksum(buffer);
}

// Pack slave->master packet (Mode + Manual RPM, reserved fields zeroed)
inline void packSlavePacket(uint8_t* buffer, uint8_t mode, uint16_t manualRpm) {
    buffer[0] = SPI_PACKET_HEADER;
    buffer[1] = manualRpm & 0xFF;
    buffer[2] = (manualRpm >> 8) & 0xFF;
    buffer[3] = mode;
    buffer[4] = 0;  // Reserved
    buffer[5] = 0;  // Reserved
    buffer[6] = 0;  // Reserved
    buffer[7] = calculateSpiChecksum(buffer);
}

// Legacy I2C support (can be removed later)
#define RPM_PACKET_HEADER SPI_PACKET_HEADER
#define RPM_PACKET_SIZE 4

inline uint8_t calculateChecksum(uint8_t header, uint16_t rpm) {
    uint8_t rpmLow = rpm & 0xFF;
    uint8_t rpmHigh = (rpm >> 8) & 0xFF;
    return header ^ rpmLow ^ rpmHigh;
}

inline bool validatePacket(const uint8_t* data, size_t len) {
    if (len != RPM_PACKET_SIZE) return false;
    if (data[0] != RPM_PACKET_HEADER) return false;
    uint16_t rpm = data[1] | (data[2] << 8);
    uint8_t expectedChecksum = calculateChecksum(data[0], rpm);
    return data[3] == expectedChecksum;
}

inline uint16_t extractRpm(const uint8_t* data) {
    return data[1] | (data[2] << 8);
}

inline void packRpmPacket(uint8_t* buffer, uint16_t rpm) {
    buffer[0] = RPM_PACKET_HEADER;
    buffer[1] = rpm & 0xFF;
    buffer[2] = (rpm >> 8) & 0xFF;
    buffer[3] = calculateChecksum(buffer[0], rpm);
}

#endif // SHARED_PROTOCOL_H
