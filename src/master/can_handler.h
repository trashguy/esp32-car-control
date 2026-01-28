#ifndef CAN_HANDLER_H
#define CAN_HANDLER_H

#include <stdint.h>

// Operating modes
enum CanMode {
    CAN_MODE_SNIFF,    // Log all messages to serial
    CAN_MODE_RPM       // Extract RPM from configured message ID
};

// Initialize MCP2515 CAN controller
bool canInit();

// Set operating mode
void canSetMode(CanMode mode);

// Set the CAN message ID to filter for RPM extraction
void canSetRpmMessageId(uint32_t messageId);

// Set byte position and scale for RPM extraction
// RPM = (data[byteOffset] | (data[byteOffset+1] << 8)) * scale
void canSetRpmExtraction(uint8_t byteOffset, float scale);

// Process incoming CAN messages
// In sniff mode: logs to serial
// In RPM mode: extracts and returns RPM value
// Returns true if new RPM value available
bool canProcess(uint16_t* rpm);

// Get statistics
uint32_t canGetMessageCount();
uint32_t canGetErrorCount();

#endif // CAN_HANDLER_H
