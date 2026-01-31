# Water Temperature Input Interface (LS1 Coolant Sensor)

This circuit interfaces a GM LS1-style NTC thermistor coolant temperature sensor with the ESP32-S3 ADC input.

## Sensor Specifications (GM LS1 CTS)

| Parameter | Value |
|-----------|-------|
| Part Number | GM 12146312 / ACDelco 213-928 |
| Type | NTC (Negative Temperature Coefficient) Thermistor |
| Configuration | Single-wire, grounded through engine block |
| Thread | 3/8" NPT |

### Resistance vs Temperature

| Temperature | Resistance | Notes |
|-------------|------------|-------|
| -40°F (-40°C) | ~100,000 Ω | Extreme cold |
| 32°F (0°C) | ~9,420 Ω | Freezing |
| 70°F (21°C) | ~3,400 Ω | Cold engine |
| 104°F (40°C) | ~1,459 Ω | Warming up |
| 176°F (80°C) | ~332 Ω | Normal operation |
| 195°F (90°C) | ~200 Ω | Thermostat open |
| 212°F (100°C) | ~177 Ω | Hot |
| 230°F (110°C) | ~100 Ω | Overheating |
| 266°F (130°C) | ~70 Ω | Critical |

## Circuit Design

### Design Goals

1. Use ESP32-S3 ADC input (0-3.3V range)
2. Maximize resolution across operating temperature range (170°F - 230°F)
3. Protect ESP32 GPIO from voltage spikes
4. Account for sensor wire capacitance and noise

### Pull-up Resistor Selection

The ESP32-S3 ADC reads 0-3.3V. Using a voltage divider with the NTC sensor:

```
Vout = Vref × (R_sensor / (R_pullup + R_sensor))
```

For optimal resolution in the operating range (200-350Ω), a **1kΩ pull-up** provides:

| Sensor State | R_sensor | Vout (3.3V ref) | ADC Value (12-bit) |
|--------------|----------|-----------------|-------------------|
| Cold (70°F) | 3,400 Ω | 2.55V | 3,161 |
| Warming (140°F) | 667 Ω | 1.32V | 1,635 |
| Normal (195°F) | 200 Ω | 0.55V | 681 |
| Hot (220°F) | 120 Ω | 0.35V | 434 |
| Overheat (230°F) | 100 Ω | 0.30V | 372 |
| Open circuit | ∞ Ω | 3.3V | 4,095 |
| Short circuit | 0 Ω | 0V | 0 |

**Note:** Higher pull-up values (e.g., 3.3kΩ) would provide better cold-range resolution but reduce hot-range resolution. 1kΩ is a good compromise for monitoring overheating conditions.

## Water Temp Input Connector (J18) - 2-Pin JST-PH

```
                ┌───────────┐
                │  1     2  │
                │  ○     ○  │
                └──┬─────┬──┘
                   │     │
                  SIG   GND
                (Sensor) (Chassis)
```

Pin Assignment:
| Pin | Signal | Description |
|-----|--------|-------------|
| 1 | SIG | Sensor signal wire (to CTS sensor) |
| 2 | GND | Chassis ground (to engine block) |

Mating Connector: JST PHR-2 (housing) + SPH-002T-P0.5S (crimp terminals)

## Complete Circuit Schematic

```
                         WATER TEMPERATURE SENSOR INTERFACE
════════════════════════════════════════════════════════════════════════════════

    Coolant Temp Sensor (LS1 CTS)
    Single-wire NTC thermistor grounded through engine block

                                    3.3V
                                     │
                                    ┌┴┐
                                R27 │ │ 1K (1/4W, 1%)
                                    │ │ Pull-up resistor
                                    └┬┘
                                     │
                                     ├──────────────────────────────────────┐
                                     │                                      │
                         ┌───────────┴───────────┐                          │
                         │                       │                          │
                        ═══ C17                 ┌┴┐                         │
                        │   100nF               │ │ R28                     │
                        │   (Filter)            │ │ 10K                     │
                        │                       └┬┘ (ESD current limit)     │
                       ─┴─                       │                          │
                       ///                       │                          │
                                                 │                          │
           Sensor Wire (J18.1) ──────────────────┼──────────────────────────┤
                    │                            │                          │
                    │                           ─┴─                         │
                    │                          ╱   ╲  D5                    │
                    │                          ─────  3.3V Zener            │
                    │                            │    (ESD/overvoltage)     │
                    │                           ─┴─                         │
                    │                           ///                         │
                    │                                                       │
                    │         RC Low-Pass Filter                            │
                    │        ┌──────────────────────┐                       │
                    │        │                      │                       │
                    └────────┤ R29 = 1K     C18 ═══ ├───────────────────────┴─► GPIO 4 (U1)
                             │              100nF  │                           (ADC1_CH3)
                             │                │    │
                             │               ─┴─   │
                             │               ///   │
                             │                     │
                             └──────────────────────┘

           Sensor Ground (J18.2) ────────────────────────────────────────────► GND
           (Chassis ground - to engine block)


    CIRCUIT EXPLANATION
    ════════════════════

    1. PULL-UP RESISTOR (R27 = 1kΩ):
       - Creates voltage divider with NTC sensor
       - 3.3V reference compatible with ESP32 ADC
       - 1kΩ provides good resolution in 100-500Ω operating range

    2. FILTER CAPACITOR (C17 = 100nF):
       - Filters high-frequency noise from engine electrical system
       - Placed close to pull-up for best filtering

    3. ESD PROTECTION (R28 + D5):
       - R28 (10kΩ) limits current during ESD events
       - D5 (3.3V Zener) clamps voltage to safe level
       - Protects GPIO from ignition noise and static discharge

    4. LOW-PASS FILTER (R29 + C18):
       - RC filter with fc = 1/(2π × 1000 × 0.0000001) ≈ 1.6kHz
       - Removes high-frequency noise while preserving slow temp changes
       - Temperature changes slowly - 1.6kHz cutoff is very conservative


    SIMPLIFIED CIRCUIT (MINIMUM PROTECTION)
    ═══════════════════════════════════════

    If space is limited, the following simplified circuit provides basic operation:

                                    3.3V
                                     │
                                    ┌┴┐
                                R27 │ │ 1K
                                    └┬┘
                                     │
    Sensor Wire (J18.1) ─────────────┼──────────────────────────────────────► GPIO 4
                                     │
                                    ═══ C17
                                    │   100nF
                                    │
                                   ─┴─
                                   ///

    Sensor Ground (J18.2) ──────────────────────────────────────────────────► GND

    Note: Simplified circuit has reduced ESD protection. Use full circuit
          for production automotive applications.
```

## GPIO Assignment

| GPIO | Function | Direction | Notes |
|------|----------|-----------|-------|
| 4 | WATER_TEMP_IN | Input | ADC1 Channel 3, 12-bit resolution |

GPIO 4 is used because:
- It supports ADC1 (required - ADC2 has WiFi conflicts)
- It was previously reserved for encoder but now available
- Located in the expansion area of the PCB

## Firmware Calibration

The NTC thermistor has a non-linear response. Use the Steinhart-Hart equation or a lookup table for accurate temperature conversion.

### Steinhart-Hart Equation

```
1/T = A + B×ln(R) + C×(ln(R))³
```

For GM LS1 CTS (approximate coefficients):
- A = 1.129148e-3
- B = 2.34125e-4
- C = 8.76741e-8

### Lookup Table (Recommended)

Pre-calculated ADC-to-temperature lookup table for faster execution:

```c
// ADC value to temperature (°F) lookup
// Assumes 12-bit ADC, 3.3V reference, 1kΩ pull-up
const struct {
    uint16_t adc_value;
    int16_t temp_f;
} water_temp_lut[] = {
    { 4000, -20 },   // Very cold / open circuit
    { 3161, 70 },    // Cold engine
    { 2500, 104 },   // Warming up
    { 1635, 140 },   // Continued warm-up
    { 1100, 170 },   // Approaching operating temp
    { 681, 195 },    // Normal operating
    { 500, 210 },    // Hot
    { 434, 220 },    // Very hot
    { 372, 230 },    // Overheating warning
    { 300, 250 },    // Critical
    { 0, 300 },      // Short circuit / sensor failure
};
```

## Design Calculations

### 1. Pull-up Current

At minimum sensor resistance (70Ω @ 266°F):
```
I_max = 3.3V / (1000Ω + 70Ω) = 3.1 mA
```
Well within sensor and resistor ratings.

### 2. Power Dissipation

Maximum power in pull-up resistor:
```
P = I² × R = (0.0031)² × 1000 = 9.6 mW
```
1/4W (250mW) resistor is adequate with large margin.

### 3. ADC Resolution

At 195°F (200Ω sensor):
```
Vout = 3.3V × 200 / (1000 + 200) = 0.55V
ADC = 0.55V / 3.3V × 4095 = 681
```

Change per 1°F near operating temp: ~15 ADC counts
This provides excellent resolution for temperature monitoring.

### 4. Filter Cutoff Frequency

RC filter: R29 = 1kΩ, C18 = 100nF
```
fc = 1 / (2π × R × C)
   = 1 / (2π × 1000 × 0.0000001)
   = 1592 Hz
```

Temperature changes occur over seconds/minutes. 1.6kHz cutoff effectively removes ignition noise (>1kHz) while passing temperature variations.

## Wiring Notes

1. **Sensor Location:**
   - LS1 has two CTS sensors - use the one connected to the gauge cluster
   - Located on driver's side cylinder head (front)
   - Single-wire sensor - case grounds through block

2. **Wire Gauge:**
   - 18-22 AWG is sufficient
   - Keep wire run under 3 meters if possible

3. **Shielding:**
   - Shielded cable recommended
   - Connect shield to GND at board end only (single-point ground)

4. **Ground Connection:**
   - J18.2 should connect to engine block ground
   - Ensure good chassis ground path

5. **Heat Exposure:**
   - Wire should be rated for engine bay temperatures
   - Use high-temp silicone or PTFE insulation wire

## Bill of Materials

| Ref | Value | Package | Qty | Notes |
|-----|-------|---------|-----|-------|
| R27 | 1kΩ 1% | 0402/0603 | 1 | Pull-up resistor |
| R28 | 10kΩ | 0402/0603 | 1 | ESD current limit |
| R29 | 1kΩ | 0402/0603 | 1 | RC filter resistor |
| C17 | 100nF | 0402/0603 | 1 | Pull-up filter |
| C18 | 100nF | 0402/0603 | 1 | RC filter capacitor |
| D5 | BZT52C3V3 | SOD-123 | 1 | 3.3V Zener (ESD clamp) |
| J18 | JST-PH 2-pin | B2B-PH-K-S | 1 | Sensor connector |

### Mating Cable Connector

| For | Housing | Crimp Terminal | Notes |
|-----|---------|----------------|-------|
| J18 | PHR-2 | SPH-002T-P0.5S | JST-PH 2-pos |

## Integration with Master PCB

### Connector Placement

Add J18 near the other vehicle interface connectors (J11 RPM, J12 VSS) on the right edge of the PCB for consistent cable routing.

### PCB Routing

- Keep analog traces away from high-speed digital (SPI, USB)
- Route ground plane under analog circuit
- Place filter capacitors as close as possible to their resistors

## Alternative Sensor Compatibility

This circuit also works with other GM NTC coolant sensors:
- LS2, LS3, LS6, LS7, LSA
- LT1, LT4, L98
- Most GM 12146312-compatible sensors
- Aftermarket sensors with similar resistance curves

For sensors with different resistance curves, adjust R27 pull-up value to optimize ADC range for the operating temperature band.
