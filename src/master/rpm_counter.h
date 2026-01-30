#ifndef RPM_COUNTER_H
#define RPM_COUNTER_H

#include <stdint.h>
#include <driver/pcnt.h>

// =============================================================================
// PCNT-Based RPM Counter
// =============================================================================
// Reads 12V square wave signal (level-shifted to 3.3V) and counts pulses
// using ESP32's hardware pulse counter for accurate, low-overhead measurement.
//
// Hardware requirements:
//   - 12V to 3.3V level shifter circuit (optocoupler recommended)
//   - 1 pulse per revolution signal
//
// PCNT resources used:
//   - PCNT_UNIT_0 (configurable)
//   - One ISR for overflow handling
// =============================================================================

// Initialize the RPM counter (does not start counting)
// Returns true on success
bool rpmCounterInit();

// Enable/disable the counter
// When disabled, releases PCNT resources and GPIO
void rpmCounterEnable();
void rpmCounterDisable();

// Check if counter is currently enabled
bool rpmCounterIsEnabled();

// Get current RPM reading
// samplePeriodMs: time window for RPM calculation (default 100ms)
// Returns 0 if disabled or no pulses detected
float rpmCounterGetRPM();

// Get raw pulse count since last call to rpmCounterGetRPM()
uint32_t rpmCounterGetPulseCount();

// Get time since last pulse (for stall detection)
// Returns UINT32_MAX if no pulses ever received
uint32_t rpmCounterGetTimeSinceLastPulse();

// Configuration
void rpmCounterSetFilterNs(uint16_t nanoseconds);  // Glitch filter (default 1000ns)
void rpmCounterSetStallTimeoutMs(uint32_t ms);     // Consider stalled after this (default 500ms)

// Statistics
uint32_t rpmCounterGetTotalPulses();   // Total pulses since enable
uint32_t rpmCounterGetOverflowCount(); // Number of counter overflows

#endif // RPM_COUNTER_H
