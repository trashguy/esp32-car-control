# 12V to 3.3V Power Supply

Simple, cheap voltage regulator circuits to power the ESP32-S3 from a 12V supply.

## Option 1: AMS1117-3.3 Linear Regulator (Simplest)

Cheapest and simplest solution. Good for low-current applications or when heat isn't a concern.

### Circuit Diagram

```
12V Input                                         3.3V Output
    │                                                  │
    │         AMS1117-3.3                              │
    │        ┌───────────┐                             │
    ├────┬───┤ IN    OUT ├───┬─────────────────────────┤
    │    │   │           │   │                         │
    │   ═══  │    GND    │  ═══                        │
    │   10µF │     │     │  10µF                       │
    │    │   └─────┼─────┘   │                         │
    │    │         │         │                         │
────┴────┴─────────┴─────────┴─────────────────────────┴──── GND
```

### AMS1117 Pinout (SOT-223 / TO-252)

```
      ┌─────────┐
 GND ─┤1       3├─ OUT
      │    2    │
      └────┬────┘
           │
          IN
```

### Parts List

| Component | Value | Notes |
|-----------|-------|-------|
| U1 | AMS1117-3.3 | 1A LDO regulator (SOT-223 or TO-252) |
| C1 | 10µF | Input capacitor (electrolytic or ceramic) |
| C2 | 10µF | Output capacitor (electrolytic or ceramic) |

**Total cost: ~$0.20-0.50**

### Heat Considerations

Power dissipation: P = (Vin - Vout) × I = (12 - 3.3) × I = **8.7 × I watts**

| ESP32-S3 Current | Power Dissipated | Notes |
|------------------|------------------|-------|
| 50mA (sleep) | 0.44W | Fine, barely warm |
| 150mA (idle) | 1.3W | Warm, OK without heatsink |
| 300mA (WiFi active) | 2.6W | Hot, needs heatsink or airflow |
| 500mA (peak) | 4.4W | Too hot, use Option 2 instead |

---

## Option 2: Two-Stage Regulator (Better Heat Distribution)

Splits the voltage drop across two regulators to reduce heat in each.

### Circuit Diagram

```
12V Input                                                      3.3V Output
    │                                                               │
    │         L7805CV                    AMS1117-3.3               │
    │        ┌────────┐                 ┌───────────┐              │
    ├───┬────┤IN  OUT ├────┬──────┬─────┤ IN    OUT ├───┬──────────┤
    │   │    │        │    │      │     │           │   │          │
    │  ═══   │  GND   │   ═══    ═══    │    GND    │  ═══         │
    │ 100nF  │   │    │  100nF  10µF    │     │     │  10µF        │
    │   │    └───┼────┘    │      │     └─────┼─────┘   │          │
    │   │        │         │      │           │         │          │
────┴───┴────────┴─────────┴──────┴───────────┴─────────┴──────────┴─── GND

                          5V (intermediate)
```

### Parts List

| Component | Value | Notes |
|-----------|-------|-------|
| U1 | L7805CV | 5V 1.5A regulator (TO-220) |
| U2 | AMS1117-3.3 | 3.3V 1A LDO (SOT-223) |
| C1 | 100nF | Input ceramic capacitor |
| C2 | 100nF | Intermediate ceramic capacitor |
| C3 | 10µF | Intermediate electrolytic |
| C4 | 10µF | Output capacitor |

**Total cost: ~$0.50-1.00**

### Heat Distribution

At 200mA load:
- **7805:** (12 - 5) × 0.2 = 1.4W
- **AMS1117:** (5 - 3.3) × 0.2 = 0.34W
- **Total:** 1.74W (same as Option 1, but spread across two packages)

---

## Option 3: Buck Converter (Most Efficient)

For high current or battery-powered applications where efficiency matters.

### Using MP1584 Module (Recommended)

Pre-built modules are extremely cheap (~$0.50-1.00) and efficient (~90%).

```
12V Input           MP1584 Module              3.3V Output
    │              ┌─────────────┐                  │
    ├──────────────┤ IN+    OUT+ ├──────────────────┤
    │              │             │                  │
    │              │ IN-    OUT- │                  │
    │              └──┬───────┬──┘                  │
    │                 │       │                     │
────┴─────────────────┴───────┴─────────────────────┴──── GND

Adjust trim pot on module to set output to 3.3V
```

### DIY Buck Using MC34063 (Through-Hole)

For those who want to build from discrete components:

```
12V Input                                                    3.3V Output
    │                                                             │
    │    D1 1N5819          L1 100µH                             │
    │    ┌──┴──┐           ┌────┐                                │
    ├────┤►    ├───────────┤    ├────┬───────────────────────────┤
    │    └─────┘     │     └────┘    │                           │
    │                │               │                           │
    │         ┌──────┴──────┐       ═══ 220µF                    │
    │         │ 8        1  │        │                           │
   ═══        │ Vcc   DrvC  │        │      R1                   │
  100µF       │             │        │     ┌───┐                 │
    │    ┌────┤ 7        2  │        ├─────┤3K3├──┐              │
    │    │    │ Ipk   SwCol │        │     └───┘  │              │
    │   ═══   │             ├────────┘            │              │
    │  330pF  │ 6        3  │                     │   R2         │
    │    │    │ Gnd    SwE  ├──────────┐         ┌┴──┐           │
    │    │    │             │          │         │1K │           │
    │    │    │ 5        4  │         ═══        └┬──┘           │
    │    │    │ Comp    TC  │        0.3Ω        │              │
    │    │    └──────┬──────┘          │          │              │
    │    │           │                 │          │              │
────┴────┴───────────┴─────────────────┴──────────┴──────────────┴── GND
```

### MC34063 Parts List

| Component | Value | Notes |
|-----------|-------|-------|
| U1 | MC34063 | Buck/boost controller (DIP-8) |
| L1 | 100µH | Inductor, >500mA rated |
| D1 | 1N5819 | Schottky diode |
| C1 | 100µF | Input electrolytic |
| C2 | 220µF | Output electrolytic |
| C3 | 330pF | Timing capacitor |
| R1 | 3.3K | Feedback divider top |
| R2 | 1K | Feedback divider bottom |
| Rsc | 0.3Ω | Current sense (optional) |

**Total cost: ~$1.50-2.00**

---

## Recommendation Summary

| Use Case | Recommended Option |
|----------|-------------------|
| Simple, low current (<150mA) | Option 1: AMS1117 |
| Moderate current, breadboard | Option 2: Two-stage |
| High current, efficiency matters | Option 3: Buck module |
| Learning/DIY, through-hole | Option 3: MC34063 |

## ESP32-S3 Power Requirements

| State | Typical Current |
|-------|-----------------|
| Deep sleep | 10-25 µA |
| Light sleep | 0.8-2 mA |
| Idle (no WiFi/BT) | 20-50 mA |
| Active (CPU busy) | 80-150 mA |
| WiFi TX | 180-310 mA |
| WiFi + BT | 200-350 mA |
| Peak (startup) | 400-500 mA |

For typical use with occasional WiFi, plan for **200-300mA average**.

## Notes

- Always add a 100nF ceramic capacitor close to the ESP32-S3 VCC pins
- If using linear regulators with 12V input, ensure adequate cooling
- For automotive use, add a TVS diode and reverse polarity protection
- The ESP32-S3 can also be powered from 5V via its onboard regulator (USB input)
