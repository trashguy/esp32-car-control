#ifndef VSS_COUNTER_H
#define VSS_COUNTER_H

#include <stdint.h>
#include <driver/pcnt.h>

// =============================================================================
// PCNT-Based Vehicle Speed Sensor (VSS) Counter
// =============================================================================
// Reads pulses from a GM 700R4 transmission Variable Reluctance (VR) speed
// sensor using ESP32's hardware pulse counter for accurate, low-overhead
// measurement.
//
// Hardware requirements:
//   - LM1815 VR sensor interface circuit
//   - GM 700R4 2-wire speed sensor (8000 PPM)
//
// PCNT resources used:
//   - PCNT_UNIT_1 (PCNT_UNIT_0 is used by RPM counter)
//   - One ISR for overflow handling
// =============================================================================

// Initialize the VSS counter (does not start counting)
// Returns true on success
bool vssCounterInit();

// Enable/disable the counter
// When disabled, releases PCNT resources and GPIO
void vssCounterEnable();
void vssCounterDisable();

// Check if counter is currently enabled
bool vssCounterIsEnabled();

// Get current speed readings
// Returns 0 if disabled or no pulses detected
float vssCounterGetMPH();    // Miles per hour
float vssCounterGetKPH();    // Kilometers per hour

// Get raw pulse count since last call
uint32_t vssCounterGetPulseCount();

// Get time since last pulse (for stopped detection)
// Returns UINT32_MAX if no pulses ever received
uint32_t vssCounterGetTimeSinceLastPulse();

// Configuration
void vssCounterSetPPM(uint16_t pulsesPerMile);    // Default: 8000 for 700R4
void vssCounterSetFilterNs(uint16_t nanoseconds); // Glitch filter (default 1000ns)
void vssCounterSetStoppedTimeoutMs(uint32_t ms);  // Consider stopped after this (default 1000ms)

// Statistics
uint32_t vssCounterGetTotalPulses();   // Total pulses since enable
uint32_t vssCounterGetOverflowCount(); // Number of counter overflows

#endif // VSS_COUNTER_H
