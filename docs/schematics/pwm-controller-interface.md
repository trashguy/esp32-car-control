# PWM Controller Interface Circuit

Interface circuit to connect the ESP32-S3 master to a standalone PWM motor controller that expects a 0-5V potentiometer input.

## Overview

The PWM controller has 3 pins (GND, Wiper, 5V) expecting a variable 0-5V signal. Since the ESP32-S3 has no DAC, we use PWM output with an RC low-pass filter to generate 0-3.3V, then amplify to 0-5V with an op-amp.

## Circuit Diagram

```
                                     12V
                                      │
ESP32-S3                              │    LM358P
┌─────────┐     RC Filter             │  ┌───────────────┐
│         │    ┌─────────┐            └─►│8 VCC          │
│  GPIO 7 ├───►│10K   ┌──┴──┐            │               │
│  (PWM)  │    └──┬───┤1µF  │───────────►│3 IN+      OUT │
│         │       │   └──┬──┘        ┌───┤2 IN-        1 ├──────► Wiper
│         │      GND    GND          │   │               │
│         │                         5.1K │4 GND          │
│         │                          │   └───────┬───────┘
│         │                          ├───────────┘
│         │                          │
│         │                         10K
│         │                          │
│     GND ├──────────────────────────┴────────────────────────► GND
└─────────┘
```

## LM358P Pinout Reference

```
┌───────────┐
│1 OUT1   8 │ VCC
│2 IN1-   7 │ OUT2
│3 IN1+   6 │ IN2-
│4 GND    5 │ IN2+
└───────────┘
```

## Parts List

| Component | Value | Notes |
|-----------|-------|-------|
| U1 | LM358P | Dual op-amp, only using one half |
| R1 | 10K | Op-amp feedback to GND |
| R2 | 5.1K | Op-amp feedback resistor |
| R3 | 10K | RC filter resistor |
| C1 | 1µF | RC filter capacitor (ceramic or film) |

## RC Filter

The RC low-pass filter converts the PWM signal to a DC voltage:

- **Cutoff frequency:** f = 1 / (2π × R × C) = 1 / (2π × 10K × 1µF) ≈ **16 Hz**
- **PWM frequency:** 5 kHz (configured in config.h)
- **Ripple:** Very low due to ~300x ratio between PWM freq and cutoff

## Op-Amp Gain Calculation

- **Gain** = 1 + (R2 / R1) = 1 + (5.1K / 10K) = **1.51**
- **Input:** 0 - 3.3V (filtered PWM)
- **Output:** 0 - 5V (to PWM controller)

## Connections Summary

| From | To |
|------|-----|
| 12V supply | LM358 pin 8 (VCC) |
| ESP32-S3 GPIO 7 | R3 (10K RC filter) |
| R3 other end | C1 (1µF) + LM358 pin 3 (IN+) |
| C1 other end | GND |
| LM358 pin 1 (OUT) | PWM board wiper + R2 (5.1K) |
| R2 other end | LM358 pin 2 (IN-) + R1 (10K) |
| R1 other end | GND |
| LM358 pin 4 | GND |
| All GNDs | Common ground |

**Note:** Leave the PWM board's 5V pin unconnected - it was meant to power a potentiometer.

## Why 12V for the Op-Amp?

The LM358 output cannot swing closer than ~1.5V from VCC. If powered from 5V, max output would only be ~3.5V. Using 12V gives plenty of headroom to reach a clean 5V output.

The LM358 is rated for up to 32V, so 12V is well within spec.

## Pin Configuration

Defined in `include/shared/config.h`:

```cpp
#define PWM_OUTPUT_PIN        7     // PWM output for motor speed control
#define PWM_OUTPUT_FREQ       5000  // 5kHz PWM frequency
#define PWM_OUTPUT_CHANNEL    0     // LEDC channel
#define PWM_OUTPUT_RESOLUTION 8     // 8-bit resolution (0-255)
```

## ESP32-S3 Code Example

```cpp
#include "shared/config.h"

void setupPwmOutput() {
    ledcSetup(PWM_OUTPUT_CHANNEL, PWM_OUTPUT_FREQ, PWM_OUTPUT_RESOLUTION);
    ledcAttachPin(PWM_OUTPUT_PIN, PWM_OUTPUT_CHANNEL);
    ledcWrite(PWM_OUTPUT_CHANNEL, 0);  // Start at 0%
}

// Set motor speed 0-100%
void setMotorSpeed(uint8_t percent) {
    uint8_t pwmValue = map(percent, 0, 100, 0, 255);
    ledcWrite(PWM_OUTPUT_CHANNEL, pwmValue);
}

// Set by RPM if you have a known range
void setMotorRpm(uint16_t rpm, uint16_t maxRpm) {
    uint8_t pwmValue = map(rpm, 0, maxRpm, 0, 255);
    ledcWrite(PWM_OUTPUT_CHANNEL, pwmValue);
}
```

## Alternative Op-Amps

If LM358 is unavailable, these rail-to-rail op-amps work on 5V single supply (no RC filter change needed, but no 12V required either):

- MCP6002
- MCP601
- TLV2371

## Notes

- ESP32-S3 does not have a DAC, unlike the original ESP32
- The RC filter + op-amp approach works reliably for motor speed control
- PWM frequency of 5kHz provides smooth output with minimal ripple after filtering
- If faster response is needed, reduce R3 to 4.7K (cutoff ~34Hz) but expect slightly more ripple
