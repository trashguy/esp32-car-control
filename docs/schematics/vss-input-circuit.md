# VSS Input Circuit (Vehicle Speed Sensor)

GM 700R4 Transmission Variable Reluctance (VR) speed sensor interface using LM1815 for reliable pulse detection across all speed ranges.

## Overview

This circuit converts the AC sine wave output from a GM 700R4 transmission's 2-wire Variable Reluctance (VR) speed sensor to a clean 3.3V digital signal suitable for the ESP32-S3's hardware pulse counter.

### Key Features

- **Adaptive threshold** via LM1815 automatically adjusts to signal amplitude
- **Works at all speeds** from parking lot crawl to highway speeds
- **Built-in hysteresis** prevents false triggering from noise
- **Minimal external components** - simpler than discrete comparator designs
- **8000 PPM (Pulses Per Mile)** sensor compatibility

## Sensor Specifications

### GM 700R4 Speed Sensor

| Parameter | Value |
|-----------|-------|
| Type | Variable Reluctance (VR) / Magnetic |
| Wires | 2 (Signal + Ground) |
| Output | AC sine wave |
| Pulses Per Mile | 8000 PPM |
| Signal Amplitude | ~0.5V (low speed) to 50V+ (high speed) |

### Expected Frequency Range

| Speed (MPH) | Pulses/Second | Frequency |
|-------------|---------------|-----------|
| 5 | 11.1 | 11.1 Hz |
| 30 | 66.7 | 66.7 Hz |
| 60 | 133.3 | 133.3 Hz |
| 80 | 177.8 | 177.8 Hz |
| 120 | 266.7 | 266.7 Hz |

Formula: `Hz = (MPH × 8000) / 3600`

## Circuit Schematic

```
                GM 700R4 VSS (2-wire VR Sensor)
                     │         │
                  Signal     Ground
                     │         │
                     │         │
                ┌────┴────┐    │
                │  TVS    │    │
                │ P6KE33CA│    │
                │(Bi-dir) │    │
                └────┬────┘    │
                     │         │
                    ─┴─       ─┴─
                    ///       ///
                    GND       GND


                                3.3V
                                 │
                    ┌────────────┼────────────────────────────────┐
                    │            │                                │
                   ═══          ┌┴┐                               │
               C14 │            │ │ R22                           │
              100nF│            │ │ 10K (pullup)                  │
                    │            └┬┘                               │
                   ─┴─            │                                │
                   ///            │                                │
                                  │                                │
                                  │    ┌────────────────────────┐  │
                                  │    │                        │  │
                                  └────┤ 8 VCC      LM1815     │  │
                                       │          (U6)         │  │
                                       │ 7 OUTPUT ─────────────┼──┼──── GPIO 38 (ESP32-S3)
                                       │                        │  │
                   ┌───────────────────┤ 6 RSET                │  │
                   │                   │                        │  │
                  ┌┴┐                  │ 5 CSET (NC)           │  │
              R23 │ │                  │                        │  │
             200K │ │                  │ 4 VR-  ────────────────┼──┼──── GND (J13.2)
                  └┬┘                  │                        │  │
                   │                   │ 3 VR+  ───┬────────────┼──┼──── VSS Signal (J13.1)
                  ─┴─                  │           │            │  │
                  ///                  │ 2 NC      │            │  │
                                       │           │            │  │
                                       │ 1 GND ────┼────────────┼──┘
                                       │           │            │
                                       └───────────┼────────────┘
                                                   │
                                                  ═══ C15
                                                  │   1nF (optional input filter)
                                                  │
                                                 ─┴─
                                                 ///
                                                 GND
```

## LM1815 Pinout

```
        ┌─────────────────┐
        │  ┌───────────┐  │
    1 ──┤  │           │  ├── 8   Pin 1: GND
        │  │  LM1815   │  │       Pin 2: NC (No Connection)
    2 ──┤  │           │  ├── 7   Pin 3: VR+ (Sensor Input +)
        │  │           │  │       Pin 4: VR- (Sensor Input -)
    3 ──┤  │           │  ├── 6   Pin 5: CSET (Internal - Leave NC)
        │  │           │  │       Pin 6: RSET (Threshold Timing)
    4 ──┤  │           │  ├── 5   Pin 7: OUTPUT (Open Collector)
        │  └───────────┘  │       Pin 8: VCC (3.3V-12V)
        └─────────────────┘
```

## VSS Input Connector (J13) - JST-PH 2-Pin

```
                ┌───────────┐
                │  1     2  │
                │  ○     ○  │
                └──┬─────┬──┘
                   │     │
                 VR+    VR-
               (Signal) (GND)
```

| Pin | Signal | Description |
|-----|--------|-------------|
| 1 | VR+ | Sensor signal (AC sine wave) |
| 2 | VR- | Sensor ground (connect to transmission case ground) |

**Mating Connector**: PHR-2 housing with SPH-002T-P0.5S crimp terminals

## Component List

| Ref | Value | Package | Description | LCSC Part |
|-----|-------|---------|-------------|-----------|
| U6 | LM1815M | SOIC-8 | VR Sensor Interface IC | C129587 |
| R22 | 10kΩ | 0402 | Output pull-up to 3.3V | C25744 |
| R23 | 200kΩ | 0402 | Adaptive threshold timing | C25811 |
| C14 | 100nF | 0402 | VCC bypass capacitor | C1525 |
| C15 | 1nF | 0402 | Input filter (optional) | C1523 |
| TVS | P6KE33CA | DO-41/SMB | Transient protection (bidirectional) | C108380 |
| J13 | B2B-PH-K-S | JST-PH 2-pin | VSS input connector | C131338 |

**Total component cost: ~$3.50**

## How It Works

### LM1815 Operation

The LM1815 is a dedicated Variable Reluctance sensor interface IC designed specifically for this application:

1. **Adaptive Zero-Crossing Detection**: The IC automatically adjusts its switching threshold based on the input signal amplitude. This allows it to work reliably from millivolt signals at very low speeds to multi-volt signals at high speeds.

2. **Built-in Hysteresis**: Prevents oscillation and false triggering when the signal crosses the threshold. The amount of hysteresis scales with signal amplitude.

3. **RSET Timing**: R23 (200kΩ) sets the adaptive threshold time constant. This value provides good general-purpose response for automotive speed sensing.

4. **Open-Collector Output**: The output pulls LOW when a pulse is detected, then releases (pulled HIGH by R22). This provides a clean digital signal compatible with the ESP32's 3.3V logic.

### Signal Flow

```
VR Sensor        ┌─────────┐        ┌─────────┐
  Output    ────►│ LM1815  │────►   │  ESP32  │
(AC Sine)        │Adaptive │        │  PCNT   │
                 │Threshold│        │(GPIO 38)│
                 └─────────┘        └─────────┘
```

### Signal Timing

```
VR Sensor Input:     /\      /\      /\      /\
(AC Sine)           /  \    /  \    /  \    /  \
                   /    \  /    \  /    \  /    \
              ────/      \/      \/      \/      \────

LM1815 Output:   ┌────┐  ┌────┐  ┌────┐  ┌────┐
(Digital)        │    │  │    │  │    │  │    │
              ───┘    └──┘    └──┘    └──┘    └────

ESP32 counts RISING or FALLING edges (configurable).
Default: FALLING edge (transition from HIGH to LOW).
```

## Design Calculations

### RSET Selection

The RSET resistor (R23) controls the adaptive threshold behavior:

- **Lower values** (50k-100k): Faster adaptation, better for rapidly changing signals
- **Higher values** (200k-500k): Slower adaptation, better noise immunity

200kΩ is a good general-purpose value for automotive speed sensing where:
- Speed changes are relatively gradual
- Noise immunity is important
- Signal amplitude varies significantly with speed

### Output Pull-up

```
I_pullup = 3.3V / 10kΩ = 0.33 mA

LM1815 output sink current: up to 20mA
0.33mA << 20mA  ✓ Reliable operation guaranteed
```

### Maximum Input Voltage

The LM1815 can handle differential inputs up to ±200V, but the TVS diode (P6KE33CA) clamps transients to ~33V to protect against load dump events on the vehicle electrical system.

### Frequency Response

The LM1815 is designed for frequencies from DC to >30kHz. For the 700R4 speed sensor:
- Maximum expected frequency at 120 MPH: ~267 Hz
- LM1815 maximum: >30,000 Hz

The circuit has significant margin for any realistic vehicle speed.

## Speed Calculation

### From Pulses to MPH

```
Speed (MPH) = (Pulses × 3600) / (PPM × Time_seconds)

Where:
- Pulses = Number of pulses counted
- PPM = 8000 (pulses per mile for 700R4)
- Time_seconds = Sampling period in seconds

Or in terms of frequency:
Speed (MPH) = (Frequency_Hz × 3600) / PPM
            = Frequency_Hz × 0.45
```

### From Pulses to KPH

```
Speed (KPH) = Speed (MPH) × 1.60934
            = Frequency_Hz × 0.724
```

## ESP32-S3 PCNT Configuration

The circuit connects to **GPIO 38** which is configured for the ESP32-S3 PCNT (Pulse Counter) peripheral.

### Why GPIO 38?

- Available GPIO not used by other peripherals
- Supports PCNT input function
- Located on expansion header for easy access during development

### Firmware Interface

```cpp
#include "vss_counter.h"

void setup() {
    vssCounterInit();
    vssCounterEnable();
}

void loop() {
    float mph = vssCounterGetMPH();
    float kph = vssCounterGetKPH();

    Serial.printf("Speed: %.1f MPH (%.1f KPH)\n", mph, kph);
    delay(100);
}
```

### Serial Commands

| Command | Action |
|---------|--------|
| `v` | Enable VSS counter / Show current speed |
| `V` | Disable VSS counter |

## Wiring Guidelines

### Sensor Connection

The GM 700R4 speed sensor has two wires:
1. **Signal wire** - Connects to J13 Pin 1 (VR+)
2. **Ground wire** - Connects to J13 Pin 2 (VR-)

Note: VR sensors are non-polarized for AC signal, but maintaining consistent wiring ensures predictable pulse direction.

### Cable Recommendations

| Parameter | Recommendation |
|-----------|----------------|
| Wire gauge | 20-22 AWG |
| Cable type | Twisted pair (shielded preferred) |
| Maximum length | 5 meters |
| Shield connection | Connect to GND at board end only |

### Installation Tips

1. Route cable away from ignition wires and spark plug cables
2. Use grommets when passing through firewalls
3. Secure cable to prevent chafing on sharp edges
4. Leave service loop near transmission for maintenance access
5. Use heat-resistant wire near transmission/exhaust

### Grounding

For best noise immunity:
- Connect J13 Pin 2 to the transmission case ground
- Ensure good ground between transmission and vehicle chassis
- Do not share ground wire with high-current loads

## Troubleshooting

### No Signal Detected

1. Verify sensor resistance (typically 200-2000Ω for VR sensors)
2. Check for ~1V AC signal at J13 with vehicle moving slowly
3. Verify LM1815 VCC is 3.3V
4. Check output pin with oscilloscope (should see pulses)

### Erratic Readings at Low Speed

1. Increase R23 value to 500kΩ for more filtering
2. Add C15 (1nF) if not installed
3. Check for loose sensor mounting (air gap too large)

### No Reading at High Speed

1. Verify LM1815 output frequency matches expected
2. Check PCNT overflow handling in firmware
3. Reduce sample interval for higher update rate

### Readings Drift or Jump

1. Check for intermittent cable connections
2. Verify sensor mounting is secure
3. Add shielding if near EMI sources
4. Check ground connection quality

## Alternative Circuits

### LM393 Comparator (Discrete)

For applications where the LM1815 is unavailable, a discrete comparator circuit can be used:

```
VR+  ────/\/\/\────┬────────────────┤ 3 IN-   LM393 ├───── Output
       10kΩ        │                │                │
                  ═══ 1nF           │         ┌──┐  │
                   │                │         │  │  │
                  GND               │     2   │  ├──┘
                                    └──/\/\/\─┴──┘
                                       10kΩ   (to GND via 10k)
                                              (to OUT via 1MΩ for hysteresis)
```

This circuit requires more components and careful tuning but works in a pinch.

### Direct GPIO (Not Recommended)

For very short cable runs and clean signals, you could use a simple voltage divider + clamp:

```
VR+ ──/\/\/\──┬──/\/\/\──┬───── GPIO
     10kΩ     │   10kΩ   │
             ═══        ─┴─ 3.3V Zener
            10nF         │
              │         GND
             GND
```

**Not recommended** - no adaptive threshold means poor low-speed performance.

## References

- [LM1815 Datasheet (Texas Instruments)](https://www.ti.com/lit/ds/symlink/lm1815.pdf)
- [ESP32-S3 PCNT Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/pcnt.html)
- GM 700R4 Transmission Service Manual
- `src/master/vss_counter.h` - Firmware API
- `src/master/vss_counter.cpp` - Firmware implementation
