# RPM Input Circuit

12V square wave to 3.3V level shifter for reading engine RPM using ESP32-S3 PCNT (Pulse Counter) peripheral.

## Overview

This circuit converts a 12V square wave signal (typically from an engine tachometer output or ignition system) to a clean 3.3V digital signal suitable for the ESP32-S3's hardware pulse counter.

### Key Features

- **Galvanic isolation** via optocoupler (PC817) protects ESP32 from automotive electrical noise
- **Reverse polarity protection** via 1N4148 diode
- **Wide input voltage range**: 8V-16V (covers dead battery to charging voltage)
- **High frequency capability**: Up to 50kHz (300,000+ RPM theoretical, well beyond any engine)
- **Low component count**: 4 components total

## Circuit Schematic

```
     12V Square Wave Input (J12 Pin 1)
            │
            │
           ┌┴┐
           │ │ R20 = 2.2kΩ
           │ │ (current limiting resistor)
           └┬┘
            │
            ├────────────┐
            │            │
            │           ─┴─
            │            ▲  D3 = 1N4148
            │           ─┬─ (reverse protection)
            │            │
            ▼ LED        │
         ┌─────────┐     │
         │  1  ○───┼─────┘
         │   PC817 │
         │  2  ○───┼──────────────────────────────── GND (J12 Pin 2)
         │   (U5)  │
         │         │  4 (Collector)
         │  4  ○───┼──────┬──────────────────────── GPIO 1 (ESP32-S3)
         │         │      │
         │  3  ○───┼──┐  ┌┴┐
         └─────────┘  │  │ │ R21 = 10kΩ
              Emitter │  │ │ (pull-up resistor)
                      │  └┬┘
                      │   │
                     GND  └──────────────────────── 3V3
```

## Component List

| Ref | Value | Package | Description | LCSC Part |
|-----|-------|---------|-------------|-----------|
| U5 | PC817C | DIP-4 or SOP-4 | Optocoupler, phototransistor output | C66463 |
| R20 | 2.2kΩ | 0402 | LED current limiting resistor | C25879 |
| R21 | 10kΩ | 0402 | Collector pull-up to 3.3V | C25744 |
| D3 | 1N4148W | SOD-123 | Reverse polarity protection | C81598 |
| J12 | B2B-PH-K-S | JST-PH 2-pin | Input connector | C131338 |

**Mating connector**: PHR-2 housing with SPH-002T-P0.5S crimp terminals

## How It Works

1. **Input Stage**: The 12V square wave drives current through R20 into the optocoupler LED
2. **Isolation**: Light from the LED activates the internal phototransistor, providing galvanic isolation
3. **Output Stage**: When the LED is ON, the transistor conducts and pulls GPIO1 LOW
4. **Logic Inversion**: The output is inverted relative to input (12V HIGH = GPIO LOW)

### Signal Timing

```
Input (12V):     ┌────┐    ┌────┐    ┌────┐
                 │    │    │    │    │    │
          ───────┘    └────┘    └────┘    └────  (tach signal)

Output (GPIO1):  ────┐    ┌────┐    ┌────┐    ┌
                     │    │    │    │    │    │
                     └────┘    └────┘    └────┘  (inverted)

ESP32 counts FALLING edges on GPIO1, which correspond to RISING edges on input.
```

## Design Calculations

### LED Current

The optocoupler LED requires current to turn on. R20 limits this current:

```
I_LED = (V_IN - V_LED) / R20
      = (12V - 1.2V) / 2.2kΩ
      = 4.9 mA (nominal)
```

| Condition | Voltage | LED Current |
|-----------|---------|-------------|
| Normal | 12V | 4.9 mA |
| Charging | 14.4V | 6.0 mA |
| Low battery | 10V | 4.0 mA |
| Minimum | 8V | 3.1 mA |

PC817 requires ~1mA minimum for reliable operation. All conditions are well above this.

### Maximum Frequency

PC817 typical rise/fall times: 4µs / 3µs

Maximum reliable frequency = 1 / (t_rise + t_fall) = 1 / 7µs ≈ **143 kHz**

At 10,000 RPM with 1 pulse/revolution: 10000/60 = **167 Hz** (well within limits)

### Pull-up Resistor Selection

```
I_pullup = 3.3V / 10kΩ = 0.33 mA

PC817 CTR (Current Transfer Ratio) = 50-300%
With 4.9mA LED current: Ic_max = 4.9mA × 50% = 2.45 mA (minimum)

0.33 mA << 2.45 mA  ✓ Reliable saturation guaranteed
```

## Input Signal Sources

### Common Tachometer Signals

| Source | Signal Type | Pulses/Rev | Notes |
|--------|-------------|------------|-------|
| Ignition coil (-) | Inductive spike | Varies | May need filtering |
| ECU tach output | Square wave | Usually 1 | Cleanest signal |
| Aftermarket tach | Square wave | 1 | Check polarity |
| Crank sensor | Sine/Square | Many | Needs tooth counting |

### Pulses Per Revolution

The firmware default assumes **1 pulse per revolution**. Common configurations:

| Engine | Pulses/Rev | Notes |
|--------|------------|-------|
| 4-cylinder (wasted spark) | 2 | Ignition fires every crank rev |
| 6-cylinder | 3 | |
| 8-cylinder | 4 | |
| Aftermarket tach adapter | 1 | Pre-divided signal |

Adjust in code: `rpmCounterSetPulsesPerRev(n)`

## ESP32-S3 PCNT Configuration

The circuit connects to **GPIO 1** which is configured for the ESP32-S3 PCNT (Pulse Counter) peripheral.

### Why PCNT?

| Method | CPU Usage | Accuracy | Max Frequency |
|--------|-----------|----------|---------------|
| Interrupt-based | High | Good | ~100 kHz |
| PCNT (hardware) | **Zero** | **Excellent** | **40 MHz** |

PCNT counts pulses in hardware with no CPU intervention, allowing accurate RPM measurement even under heavy CPU load.

### Firmware Interface

```cpp
#include "rpm_counter.h"

void setup() {
    rpmCounterInit();
    rpmCounterEnable();
}

void loop() {
    float rpm = rpmCounterGetRPM();
    Serial.printf("RPM: %.0f\n", rpm);
    delay(100);
}
```

### Serial Commands

| Command | Action |
|---------|--------|
| `p` | Enable counter / Show current RPM |
| `P` | Disable counter |

## Wiring Guidelines

### Cable Selection

- **Wire gauge**: 22-24 AWG (signal level current only)
- **Cable type**: Twisted pair recommended for noise rejection
- **Shielding**: Use shielded cable in high-noise environments

### Installation

1. Connect J12 Pin 1 to tachometer signal source
2. Connect J12 Pin 2 to signal ground (chassis ground)
3. Route cable away from ignition wires and spark plug cables
4. Keep cable length under 2 meters if possible

### Grounding

The optocoupler provides **galvanic isolation** between the vehicle electrical system and the ESP32. This means:

- Vehicle ground and ESP32 ground are electrically separate
- Ground loops are prevented
- Voltage spikes on vehicle ground won't affect ESP32

However, J12 Pin 2 must still share a common reference with the signal source for proper operation.

## Troubleshooting

### No Signal Detected

1. Verify input voltage with multimeter (should see 0-12V square wave)
2. Check LED current: measure voltage across R20 (should be ~10.8V when HIGH)
3. Verify pull-up: GPIO1 should read 3.3V when no input signal
4. Check optocoupler orientation (dot/notch indicates pin 1)

### Incorrect RPM Reading

1. Verify pulses-per-revolution setting matches your signal source
2. Check for signal bounce (increase filter time in firmware)
3. Verify sample rate is appropriate for RPM range

### Erratic Readings

1. Add shielded cable if near ignition components
2. Increase glitch filter: `rpmCounterSetFilterNs(2000)` (2µs)
3. Check for loose connections at J12

## Alternative Circuits

### Transistor Level Shifter (No Isolation)

Simpler but **not recommended for automotive use**:

```
     12V Input
        │
       ┌┴┐
       │ │ 10kΩ
       └┬┘
        │
        ├──────┬──────── GPIO 1
        │      │
       ┌┴┐    ┌┴┐
  22kΩ │ │    │ │ 10kΩ (to 3.3V)
       └┬┘    └┬┘
        │      │
       ─┴─     └─── 3V3
      3.3V Zener
        │
       GND
```

This circuit lacks isolation and may allow noise to affect the ESP32.

### High-Speed Optocoupler (6N137)

For frequencies above 50kHz, use 6N137:

- Rise/fall time: 23ns typical
- Maximum frequency: 10 MHz+
- Requires different circuit (has internal pull-up, needs output capacitor)

Not necessary for typical engine RPM signals.

## References

- [PC817 Datasheet](https://www.farnell.com/datasheets/73758.pdf)
- [ESP32-S3 PCNT Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/pcnt.html)
- `src/master/rpm_counter.h` - Firmware API
- `src/master/rpm_counter.cpp` - Firmware implementation
