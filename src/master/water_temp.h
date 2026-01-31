#ifndef WATER_TEMP_H
#define WATER_TEMP_H

#include <stdint.h>

// =============================================================================
// ADC-Based Water Temperature Sensor (GM LS1 Coolant Temperature Sensor)
// =============================================================================
// Reads GM LS1 NTC thermistor via voltage divider circuit:
//   3.3V -> R27 (1K pull-up) -> Sensor Junction -> NTC Sensor -> GND
//
// The NTC thermistor resistance decreases as temperature increases.
// ADC reads the voltage at the junction, which is then converted to
// resistance, then to temperature using the Steinhart-Hart equation
// or lookup table for the specific GM LS1 sensor characteristics.
//
// Hardware requirements:
//   - 1K pull-up resistor to 3.3V (R27)
//   - 10K ESD current limiting resistor (R28)
//   - 3.3V Zener diode for overvoltage protection (D5)
//   - RC filter: R29 (1K) + C18 (100nF) for noise rejection
//
// See docs/schematics/water-temp-input-circuit.md for full circuit details.
// =============================================================================

// Initialize the water temperature sensor (does not start reading)
// Returns true on success
bool waterTempInit();

// Enable/disable the sensor
// When disabled, releases ADC resources
void waterTempEnable();
void waterTempDisable();

// Check if sensor is currently enabled
bool waterTempIsEnabled();

// Get current temperature reading in Fahrenheit
// Returns NAN if disabled or sensor error
float waterTempGetFahrenheit();

// Get current temperature reading in Celsius
// Returns NAN if disabled or sensor error
float waterTempGetCelsius();

// Get raw ADC value (0-4095 for 12-bit ADC)
// Useful for diagnostics and calibration
uint16_t waterTempGetRawADC();

// Get calculated sensor resistance in ohms
// Useful for diagnostics
float waterTempGetResistanceOhms();

// Get voltage at sensor junction (0.0 - 3.3V)
// Useful for diagnostics
float waterTempGetVoltage();

// Configuration
void waterTempSetAveraging(uint8_t samples);  // Number of samples to average (1-64, default 16)
void waterTempSetUpdateRateMs(uint16_t ms);   // Minimum time between readings (default 100ms)

// Calibration
// Offset is added to the final temperature reading
void waterTempSetOffsetF(float offsetF);      // Calibration offset in Fahrenheit
float waterTempGetOffsetF();

// Sensor health/diagnostics
bool waterTempIsSensorConnected();            // Returns false if open circuit detected
bool waterTempIsSensorShorted();              // Returns false if short circuit detected

// Statistics
uint32_t waterTempGetReadCount();             // Total successful readings
uint32_t waterTempGetErrorCount();            // Total read errors

#endif // WATER_TEMP_H
