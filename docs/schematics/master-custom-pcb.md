# Master Custom PCB Schematic

Custom PCB design for the ESP32-S3 master board with integrated USB programming, power regulation, and all peripheral interfaces.

## Design Overview

This schematic replaces the ESP32-S3-DevKitC-1 with a custom PCB featuring:
- ESP32-S3-WROOM-1-N16R8 module (16MB Flash, 8MB PSRAM)
- Single USB-C connector using ESP32-S3 native USB
- 12V automotive input with efficient power regulation
- Integrated CAN bus transceiver (replaces MCP2515 module)
- On-board PWM-to-analog conversion circuit
- MCP23017 I2C GPIO expander for up to 5 rotary encoders
- Dedicated power steering speed control encoder
- SPI connector for slave communication
- Micro SD card slot for data logging
- 12V RPM input with optocoupler isolation for engine tachometer signal
- Vehicle Speed Sensor (VSS) (Dakota Digital) input with LM1815 VR sensor interface for GM 700R4 transmission

## Connector Summary

All external peripheral connections use locking cable connectors for reliable automotive use:

| Connector | Type | Pins | Function |
|-----------|------|------|----------|
| J1 | Screw Terminal (5.08mm) | 2 | 12V Power Input |
| J2 | USB-C Receptacle | - | Programming/Debug (Native USB) |
| J3 | JST-XH 7-pin | 7 | MCP2515 CAN Module |
| J4 | JST-GH 4-pin | 4 | CAN Bus (CANH/CANL/12V/GND) |
| J5 | JST-PH 3-pin | 3 | PWM Output (SIG/5V/GND) |
| J6 | JST-GH 6-pin | 6 | SPI Slave Communication |
| J7 | Micro SD Slot | - | SD Card Storage |
| J8 | Pin Header 2×7 | 14 | Expansion GPIOs |
| J9 | ARM Cortex 2×5 (1.27mm) | 10 | JTAG Debug Interface |
| J10 | Pin Header 1×6 (2.54mm) | 6 | UART Debug + Boot Control |
| J11 | JST-PH 2-pin | 2 | RPM Input (12V Square Wave) |
| J12 | JST-PH 2-pin | 2 | VSS Input (700R4 VR Sensor) |
| J13 | JST-XH 5-pin | 5 | Encoder 1 - Power Steering Speed (via MCP23017) |
| J14 | JST-XH 5-pin | 5 | Encoder 2 - Future Use (via MCP23017) |
| J15 | JST-XH 5-pin | 5 | Encoder 3 - Future Use (via MCP23017) |
| J16 | JST-XH 5-pin | 5 | Encoder 4 - Future Use (via MCP23017) |
| J17 | JST-XH 5-pin | 5 | Encoder 5 - Future Use (via MCP23017) |
| J18 | JST-PH 2-pin | 2 | Water Temp Input (LS1 CTS Sensor) |

## System Block Diagram

```
                    ┌─────────────────────────────────────────────────────────────────────────────────────┐
     12V IN         │                           MASTER CUSTOM PCB                                         │
        │           │                                                                                     │
        ▼           │   ┌─────────┐    ┌─────────────────────────────────────┐                           │
   ┌─────────┐      │   │  USB-C  │    │     ESP32-S3-WROOM-1-N16R8         │                           │
   │ MP2359  │──3.3V──►│  (J2)   │◄──►│                                     │                           │
   │  Buck   │      │   │         │    │  GPIO 19/20: USB D+/D-             │                           │
   └────┬────┘      │   └─────────┘    │  GPIO 1: RPM Input (PCNT)          │                           │
        │           │                   │  GPIO 4: Water Temp (ADC)          │                           │
        │           │                   │  GPIO 38: VSS Input (PCNT)         │                           │
       12V──────────│───────────────────│  GPIO 47/48: I2C (MCP23017)        │                           │
        │           │   ┌─────────┐    │  SPI1: MCP2515 (CAN)               │                           │
        │           │   │ MCP2515 │◄──►│  SPI2: Slave Communication          │                           │
        │           │   │   CAN   │    │  SPI3: SD Card                      │                           │
        │           │   │ + TJA   │    │  GPIO 7: PWM Output                 │                           │
        │           │   │  (J3)   │    └──────────────┬──────────────────────┘                           │
        │           │   └────┬────┘     ┌─────────────┼─────────────┐                                    │
        │           │        │          │             │             │                                    │
        ▼           │        ▼          ▼             ▼             ▼                                    │
   ┌─────────┐      │   ┌─────────┐  ┌───────┐  ┌─────────┐  ┌────────────────────┐  ┌───────────────┐  │
   │  LM358  │      │   │ JST-GH  │  │ PC817 │  │ Micro SD│  │     MCP23017       │  │  Water Temp   │  │
   │ Op-Amp  │      │   │CAN (J4) │  │ Opto  │  │  (J7)   │  │  I2C GPIO Expander │  │   Interface   │  │
   └────┬────┘      │   └────┬────┘  │ (U5)  │  └─────────┘  │       (U7)         │  │  R27+C17+D5   │  │
        │           │        │       └───┬───┘               │                    │  └───────┬───────┘  │
        ▼           │        │           │       ┌───────┐   │ GPA0-2: ENC1 (J13) │          │          │
   ┌─────────┐      │        │       ┌───┴───┐   │LM1815 │   │ GPA3-5: ENC2 (J14) │      ┌───┴───┐      │
   │ JST-PH  │      │        │       │JST-PH │   │ VR IF │   │ GPA6-7,│           │      │JST-PH │      │
   │PWM (J5) │      │        │       │RPM J11│   │ (U6)  │   │ GPB0:   ENC3 (J15) │      │WT J18 │      │
   └────┬────┘      │        │       └───┬───┘   └───┬───┘   │ GPB1-3: ENC4 (J16) │      └───┬───┘      │
        │           │        │           │       ┌───┴───┐   │ GPB4-6: ENC5 (J17) │          │          │
        ▼           │        ▼           ▼       │JST-PH │   └─────────┬──────────┘          ▼          │
   Motor Ctrl       │    CAN Bus     Engine RPM  │VSS J12│             │                  Coolant       │
   (0-5V PWM)       │  (CANH/CANL)   (12V Tach)  └───┬───┘    ┌────────┴────────┐         Temp          │
                    │                                │        │                 │       (LS1 CTS)       │
                    │                                ▼        ▼                 ▼                       │
                    │                          Vehicle    ┌───────┐  ┌───────┐  ┌───────┐  ┌─────────┐  │
                    │                           Speed     │ENC 1-5│  │ J13-  │  │Rotary │  │  Debug  │  │
                    │                       (700R4 VR)    │via I2C│  │  J17  │  │Encoders  │ J9/J10  │  │
                    │                                     └───────┘  └───────┘  └───────┘  └─────────┘  │
                    │                                                                                     │
                    │   Encoder Functions: J13=Power Steering, J14-J17=Future Expansion                  │
                    │   Sensor Inputs: J11=RPM, J12=VSS, J18=Water Temp                                  │
                    └─────────────────────────────────────────────────────────────────────────────────────┘
```

## Complete Schematic

### Sheet 1: Power Supply

```
                                    POWER SUPPLY
════════════════════════════════════════════════════════════════════════════════════

    12V IN (J1)
    ┌─────────┐
    │ + ─ GND │
    └─┬───┬───┘
      │   │
      │   └────────────────────────────────────────────────────────────────────► GND
      │
      ├─────────────────────────────────────────────────────────────────────────► 12V_RAIL
      │
      │   TVS Diode (Reverse polarity & surge protection)
      ├───┤◄├─── GND
      │  SMBJ15A
      │
      │         Schottky Diode (Reverse polarity)
      ├────┤►├────┬───────────────────────────────────┐
      │   SS34    │                                   │
      │           │                                   │
      │          ═══ C1                               │
      │          22µF/25V                             │
      │           │                                   │
      │          GND                                  │
      │                                               │
      │                                               │
      │   ┌───────────────────────────────────────────┴───────────────┐
      │   │              MP2359 BUCK CONVERTER                        │
      │   │              (or equivalent: MP1584, TPS562200)           │
      │   │                                                           │
      │   │         ┌─────────────┐                                   │
      │   │   VIN ──┤1 MP2359   5├── BST ──┬── 100nF ── SW            │
      │   │         │           4├── SW ───┴───┐                      │
      │   │   GND ──┤2         3├── FB        │                      │
      │   │         └─────────────┘    │       │                      │
      │   │                            │      ═══ L1                   │
      │   │                       ┌────┴──┐   10µH                     │
      │   │                       │       │    │                      │
      │   │                      R1      R2    ├───────────────────► 3V3_RAIL
      │   │                     47K     15K    │                      │
      │   │                       │       │   ═══ C2                   │
      │   │                       └───┬───┘   22µF/10V                 │
      │   │                           │        │                      │
      │   │                          GND      GND                     │
      │   │                                                           │
      │   │   Output: 3.3V @ up to 1.2A                               │
      │   │   Feedback: R1/(R1+R2) sets output voltage                │
      │   │   Vout = 0.6V × (1 + R1/R2) = 0.6 × (1 + 47/15) = 2.48V  │
      │   │   Adjusted: R1=33K, R2=10K gives 3.3V                    │
      │   └───────────────────────────────────────────────────────────┘
      │
      │
      └──────────────────────────────────────────────────────────────────────────► 12V_RAIL

    ALTERNATIVE: Linear Regulator (simpler, less efficient)
    ════════════════════════════════════════════════════════
    If heat dissipation acceptable (~2W at 200mA), use:

              AMS1117-3.3
             ┌───────────┐
    12V_IN ──┼─┤ IN    OUT ├─┬──► 3V3_RAIL
             │ │           │ │
            ═══│    GND    │═══
           10µF│     │     │10µF
             │ └─────┼─────┘ │
            GND     GND     GND

    Note: Linear regulator wastes (12V-3.3V) × current as heat
          At 200mA: 1.74W dissipation - needs thermal relief
```

### Sheet 2: ESP32-S3 Module

```
                            ESP32-S3-WROOM-1-N16R8 MODULE
════════════════════════════════════════════════════════════════════════════════════

                              Module Top View
                    ┌───────────────────────────────────────┐
                    │    ▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄     │
                    │    █  ESP32-S3-WROOM-1-N16R8   █     │
                    │    █                           █     │
                    │    █     Integrated Antenna    █     │
                    │    █                           █     │
                    │    ▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀     │
                    │                                       │
                    │ 1  2  3  4  5  6  7  8  9  10 11 12  │
                    │ ○  ○  ○  ○  ○  ○  ○  ○  ○  ○  ○  ○   │
                    └─┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬───┘
                      │  │  │  │  │  │  │  │  │  │  │  │
       GND ───────────┘  │  │  │  │  │  │  │  │  │  │  └─── GND
       3V3 ──────────────┘  │  │  │  │  │  │  │  │  └────── IO21 (COMM_CS)
       EN ──────────────────┘  │  │  │  │  │  │  └───────── IO20 (USB_D+)
       IO4 (ENC_CLK) ──────────┘  │  │  │  │  └──────────── IO19 (USB_D-)
       IO5 (ENC_DT) ──────────────┘  │  │  └─────────────── IO18 (NC)
       IO6 (ENC_SW) ──────────────────┘  └────────────────── IO17 (NC)
       IO7 (PWM_OUT) ─────────────────────────────────────── ...

                    ┌───────────────────────────────────────┐
                    │ 24 23 22 21 20 19 18 17 16 15 14 13   │
                    │ ○  ○  ○  ○  ○  ○  ○  ○  ○  ○  ○  ○    │
                    └─┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬────┘
                      │  │  │  │  │  │  │  │  │  │  │  │
       IO0 ───────────┘  │  │  │  │  │  │  │  │  │  │  └─── IO8
       IO1 ──────────────┘  │  │  │  │  │  │  │  │  └────── IO9 (MCP_INT)
       IO2 (COMM_MOSI) ─────┘  │  │  │  │  │  │  └───────── IO10 (MCP_CS)
       IO3 (COMM_MISO) ────────┘  │  │  │  │  └──────────── IO11 (MCP_MOSI)
       IO14 (COMM_SCK) ────────────┘  │  │  └─────────────── IO12 (MCP_SCK)
       NC ─────────────────────────────┘  └────────────────── IO13 (MCP_MISO)


    BOOT/RESET CIRCUIT
    ══════════════════

                                  3V3
                                   │
                                  ┌┴┐
                                  │ │ 10K
                                  └┬┘
                                   │
    EN ────────────────────────────┼─────────┬──────────────────────► To ESP32 EN
                                   │         │
                                  ═══       ─┴─
                                 100nF     ╱   ╲  Reset Button (SW1)
                                   │      ─┬───┬─
                                  GND       │
                                           GND


                                  3V3
                                   │
                                  ┌┴┐
                                  │ │ 10K
                                  └┬┘
                                   │
    IO0 ───────────────────────────┼─────────┬──────────────────────► To ESP32 IO0
                                   │         │
                                  ═══       ─┴─
                                 100nF     ╱   ╲  Boot Button (SW2)
                                   │      ─┬───┬─  (Optional - USB auto-boot)
                                  GND       │
                                           GND


    DECOUPLING CAPACITORS
    ═════════════════════

    Place as close as possible to ESP32-S3 module pins:

         3V3 ─────┬────────┬────────┬────────┬──────► ESP32 3V3
                 ═══      ═══      ═══      ═══
                100nF    100nF    100nF     10µF
                  │        │        │        │
                 GND      GND      GND      GND

    Minimum: 3× 100nF ceramic + 1× 10µF ceramic/tantalum
```

### Sheet 3: USB-C Interface (Native USB)

```
                              USB-C CONNECTOR (Native USB Mode)
════════════════════════════════════════════════════════════════════════════════════

    The ESP32-S3 has built-in USB functionality - no external UART chip needed!
    Uses GPIO 19 (D-) and GPIO 20 (D+)

                            USB-C Receptacle (J2)
                          ┌─────────────────────────┐
                          │   A1  A2  A3  A4  A5    │
                          │   ○   ○   ○   ○   ○     │
                          │                         │
                          │   B12 B11 B10 B9  B8    │
                          │   ○   ○   ○   ○   ○     │
                          │                         │
                          │   A12 A11 A10 A9  A8    │
                          │   ○   ○   ○   ○   ○     │
                          │                         │
                          │   B1  B2  B3  B4  B5    │
                          │   ○   ○   ○   ○   ○     │
                          └────────┬────────────────┘
                                   │
    USB-C Pin Connections (USB 2.0 only):
    ═════════════════════════════════════

    Pin A1, B1, A12, B12 (GND) ─────────────────────────────────────► GND
    Pin A4, B4 (VBUS) ─────┬───────────────────────────────────────► VBUS (5V from USB)
                           │
                          ═══ 10µF
                           │
                          GND

    Pin A5 (CC1) ──────────┬──────► 5.1K ──────► GND  (UFP identification)
    Pin B5 (CC2) ──────────┴──────► 5.1K ──────► GND

    Pin A6, B6 (D+) ────┬───────────────────────────────────────────► GPIO 20 (USB_D+)
                        │
                       ┌┴┐
                       │ │ 22Ω (series termination, optional)
                       └┬┘
                        │
                       ───
                       ─┴─ ESD Protection
                        │   (USBLC6-2)
                       ───
                        │
                       GND

    Pin A7, B7 (D-) ────┬───────────────────────────────────────────► GPIO 19 (USB_D-)
                        │
                       ┌┴┐
                       │ │ 22Ω (series termination, optional)
                       └┬┘
                        │
                       ───
                       ─┴─ ESD Protection
                        │
                       ───
                        │
                       GND


    USB POWER PATH (VBUS Input)
    ═══════════════════════════

    When powered via USB (development/programming):

    VBUS (5V) ────┬─────────────────────────────────────► 5V_USB
                  │
                 ─┴─
                ╱   ╲ D1
               ─────── Schottky BAT54
                  │
                  └──────────────────────────────────────► Can power 3.3V regulator
                                                           (select via jumper or auto-switch)


    AUTO-PROGRAMMING CIRCUIT (Optional but recommended)
    ═══════════════════════════════════════════════════

    DTR/RTS from USB-CDC can auto-reset into bootloader:

    USB_DTR ──────┬─────────────────────────────────────────────────►
                  │
                 ───
                 ─┬─ 100nF
                  │
                  ├──────┐
                  │      │
                 ┌┴┐    ┌┴┐
             10K │ │    │ │ 10K
                 └┬┘    └┬┘
                  │      │
                  │      └──── ESP32 IO0
                  │
                  └─────────── ESP32 EN

    Note: ESP32-S3 native USB supports auto-reset via USB-CDC protocol
          No external transistors needed if using native USB
```

### Sheet 4: CAN Bus Interface

```
                              CAN BUS INTERFACE (MCP2515 + TJA1050)
════════════════════════════════════════════════════════════════════════════════════

    Option A: Using MCP2515 Module (Simpler)
    ════════════════════════════════════════

    Just provide header pins for off-the-shelf MCP2515 module:

                    MCP2515 Module Header (J3)
                    ┌─────────────────────────────┐
                    │  VCC  GND  CS  SO  SI  SCK  INT  │
                    │   ○    ○   ○   ○   ○   ○    ○    │
                    └───┬────┬───┬───┬───┬───┬────┬────┘
                        │    │   │   │   │   │    │
                       3V3  GND  │   │   │   │    │
                                 │   │   │   │    │
                          GPIO10─┘   │   │   │    └──GPIO9
                               GPIO13┘   │   │
                                   GPIO11┘   │
                                       GPIO12┘


    Option B: Integrated CAN (Component-level)
    ═══════════════════════════════════════════

         3V3                                            12V_RAIL
          │                                                │
         ─┴─                                              ─┴─
        ╱   ╲ Ferrite bead                               ╱   ╲
       ───────                                          ───────
          │                                                │
          │      MCP2515                                   │      TJA1050
          │     ┌────────────────┐                         │     ┌────────────────┐
          ├─────┤1 VDD    TXCAN 8├─────────────────────────┼─────┤1 TXD    VCC  8├─┘
          │     │                │                         │     │               │
    GPIO10├─────┤2 CS     RXCAN 7├─────────────────────────┼─────┤4 RXD    GND  2├──GND
          │     │                │                         │     │               │
    GPIO12├─────┤3 SCK     CLKOT 6├─(NC)                   │     │        CANH  7├───► CANH
          │     │                │                         │     │               │     (J4)
    GPIO11├─────┤4 SI      INT  5├─────GPIO9               │     │        CANL  6├───► CANL
          │     │                │                         │     │               │
    GPIO13├─────┤5 SO     RESET 4├─────3V3 (or GPIO)       │     │          S   5├──GND
          │     │                │                         │     │               │
          │     │          OSC1 13├───┬───────────────     │     │        REF   3├──┬── 100nF ── GND
          │     │                │   │    ┌──┐             │     └────────────────┘  │
          │     │          OSC2 14├───┼────┤  ├───GND      │                        GND
          │     │                │   │    │  │ 8MHz
          │     │           VSS 12├──GND  │  │ Crystal
          │     └────────────────┘   │    └──┘
          │                          │     │
          │                         ═══   ═══
          │                        22pF  22pF (load capacitors)
          │                          │     │
          │                         GND   GND
          │
         ═══ 100nF (decoupling)
          │
         GND


    CAN BUS CONNECTOR (J4) - JST-GH 4-Pin Locking Connector
    ════════════════════════════════════════════════════════

    JST-GH (1.25mm pitch) - compact, locking, automotive-grade

                    JST-GH-4P (J4)
                    ┌─────────────────────┐
                    │ ┌───┬───┬───┬───┐   │
                    │ │ 1 │ 2 │ 3 │ 4 │   │  ← Locking tab
                    │ └─┬─┴─┬─┴─┬─┴─┬─┘   │
                    └───┼───┼───┼───┼─────┘
                        │   │   │   │
                      CANH CANL 12V GND

    Pin Assignment:
    ┌─────┬────────┬─────────────────────────────────────┐
    │ Pin │ Signal │ Description                         │
    ├─────┼────────┼─────────────────────────────────────┤
    │  1  │ CANH   │ CAN High (to vehicle bus)           │
    │  2  │ CANL   │ CAN Low (to vehicle bus)            │
    │  3  │ 12V    │ Optional 12V pass-through for OBD   │
    │  4  │ GND    │ Ground reference                    │
    └─────┴────────┴─────────────────────────────────────┘

    Mating Connector: JST GHR-04V-S (housing) + SSH-003T-P0.2 (crimp terminals)
    Wire: 22-26 AWG twisted pair for CANH/CANL

    Optional 120Ω termination (install jumper JP1 if end-of-bus):

                      CANH───┬───CANL
                            ┌┴┐
                        JP1 │ │ 120Ω (0805 SMD or through-hole)
                            └┬┘
                             │
                            GND (via 100nF for split termination)
```

### Sheet 5: PWM Motor Controller Interface

```
                              PWM TO ANALOG CONVERSION (0-5V Output)
════════════════════════════════════════════════════════════════════════════════════

    ESP32-S3 has no DAC, so we use:
    1. PWM output (5kHz, 8-bit)
    2. RC low-pass filter (converts to 0-3.3V DC)
    3. Non-inverting op-amp (amplifies to 0-5V)

                        RC FILTER              OP-AMP AMPLIFIER
                       ┌─────────┐            ┌─────────────────┐
    GPIO 7 ────────────┤         ├────────────┤                 ├──────► OUTPUT
    (PWM 5kHz)         │ 10K+1µF │            │ LM358 Gain=1.51 │      (0-5V DC)
                       └─────────┘            └─────────────────┘


    DETAILED CIRCUIT
    ════════════════

                                                               12V_RAIL
                                                                  │
                                                                  │
                                        LM358 (U4)                │
                                       ┌────────────┐             │
                                       │            │             │
                       RC Filter       │  8  VCC ───┼─────────────┘
                      ┌────────┐       │            │
    GPIO 7 ───────────┤►  R1   ├───┬───┤► 3+ OUT 1 ├───┬──────────────────► PWM_OUT (J5)
    (PWM)             │  10K   │   │   │            │   │                    (0-5V)
                      └────────┘   │   │    2-  ◄──┼───┤
                                   │   │            │   │
                                  ═══  │  4  GND ──┼───│───────────────────► GND
                                 C1│   │            │   │
                                 1µF   └────────────┘   │
                                   │                    │
                                   │                   ┌┴┐
                                   │               R3  │ │ 5.1K
                                   │                   └┬┘
                                  GND                   │
                                                        ├──────────────────► Feedback
                                                        │
                                                       ┌┴┐
                                                   R2  │ │ 10K
                                                       └┬┘
                                                        │
                                                       GND


    CALCULATIONS
    ════════════

    RC Filter Cutoff Frequency:
    fc = 1 / (2π × R1 × C1)
    fc = 1 / (2π × 10000 × 0.000001)
    fc = 15.9 Hz

    PWM frequency = 5000 Hz >> 15.9 Hz (good filtering)

    Op-Amp Gain (Non-inverting):
    Gain = 1 + (R3 / R2)
    Gain = 1 + (5100 / 10000)
    Gain = 1.51

    Output Voltage Range:
    Input: 0V to 3.3V (from ESP32 PWM filtered)
    Output: 0V × 1.51 to 3.3V × 1.51
    Output: 0V to 4.98V ≈ 0-5V


    PWM OUTPUT CONNECTOR (J5) - JST-PH 3-Pin Locking Connector
    ═══════════════════════════════════════════════════════════

    JST-PH (2.0mm pitch) - compact, secure, common in hobby electronics

                    JST-PH-3P (J5)
                    ┌─────────────────────┐
                    │ ┌───┬───┬───┐       │
                    │ │ 1 │ 2 │ 3 │       │  ← Locking tab
                    │ └─┬─┴─┬─┴─┬─┘       │
                    └───┼───┼───┼─────────┘
                        │   │   │
                       SIG  5V  GND
                     (0-5V)

    Pin Assignment:
    ┌─────┬────────┬─────────────────────────────────────┐
    │ Pin │ Signal │ Description                         │
    ├─────┼────────┼─────────────────────────────────────┤
    │  1  │ SIG    │ Analog 0-5V output (from op-amp)    │
    │  2  │ 5V     │ 5V reference (optional, from reg)   │
    │  3  │ GND    │ Ground                              │
    └─────┴────────┴─────────────────────────────────────┘

    Mating Connector: JST PHR-3 (housing) + SPH-002T-P0.5S (crimp terminals)
    Wire: 22-26 AWG

    Usage Notes:
    - SIG connects to motor controller "wiper" or analog speed input
    - 5V pin provides reference if motor controller needs it
    - Some controllers may not need 5V - leave disconnected if unused
```

### Sheet 6: Slave Communication (SPI)

```
                              SPI INTERFACE TO SLAVE BOARD
════════════════════════════════════════════════════════════════════════════════════

    Full-duplex SPI at 1MHz for master-slave communication

    SPI CONNECTOR (J6) - JST-GH 6-Pin Locking Connector
    ═══════════════════════════════════════════════════

    JST-GH (1.25mm pitch) - compact, locking, reliable for data cables

                    JST-GH-6P (J6)
                    ┌─────────────────────────────┐
                    │ ┌───┬───┬───┬───┬───┬───┐   │
                    │ │ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │   │  ← Locking tab
                    │ └─┬─┴─┬─┴─┬─┴─┬─┴─┬─┴─┬─┘   │
                    └───┼───┼───┼───┼───┼───┼─────┘
                        │   │   │   │   │   │
                      MOSI MISO SCK  CS  3V3 GND

    Pin Assignment:
    ┌─────┬────────┬─────────────────────────────────────┐
    │ Pin │ Signal │ Description                         │
    ├─────┼────────┼─────────────────────────────────────┤
    │  1  │ MOSI   │ Master Out, Slave In (GPIO 2)       │
    │  2  │ MISO   │ Master In, Slave Out (GPIO 3)       │
    │  3  │ SCK    │ Serial Clock (GPIO 14)              │
    │  4  │ CS     │ Chip Select, active LOW (GPIO 21)   │
    │  5  │ 3V3    │ 3.3V power (optional for slave)     │
    │  6  │ GND    │ Ground reference                    │
    └─────┴────────┴─────────────────────────────────────┘

    Mating Connector: JST GHR-06V-S (housing) + SSH-003T-P0.2 (crimp terminals)

    Cable Recommendations:
    - Length: < 30cm for reliable 1MHz operation
    - Type: 6-conductor shielded cable (shield to GND)
    - Alternative: Ribbon cable with ground between signals

    Pre-made Cable Option:
    - Sparkfun Qwiic cables (JST-SH) can be adapted
    - Or use 1.25mm pitch flat flex cable (FFC)


    OPTIONAL: Series Termination for EMI (on PCB)
    ═════════════════════════════════════════════

                      33Ω
    GPIO 2  ─────────┤►├────────────────────► J6 Pin 1 (MOSI)

                      33Ω
    GPIO 14 ─────────┤►├────────────────────► J6 Pin 3 (SCK)

                                    3V3
                                     │
                                    ┌┴┐
                                R26 │ │ 10K (CS pull-up)
                                    └┬┘
                      33Ω            │
    GPIO 21 ─────────┤►├────────────┴───────► J6 Pin 4 (CS)

    GPIO 3  ◄──────────────────────────────── J6 Pin 2 (MISO)
                                              (no termination on input)

    Note: R26 keeps CS HIGH during ESP32 boot/reset, preventing the slave
          from receiving spurious SPI data while the master GPIO is floating.
```

### Sheet 7: Micro SD Card Interface

```
                              MICRO SD CARD INTERFACE (SPI Mode)
════════════════════════════════════════════════════════════════════════════════════

    SD card in SPI mode for data logging and configuration storage.
    Uses dedicated SPI bus on available GPIOs.

    SD CARD PINOUT (J7)
    ═══════════════════

                         Micro SD Card Slot
                    ┌─────────────────────────────┐
                    │  ┌─────────────────────┐    │
                    │  │                     │    │
                    │  │   ┌─────────────┐   │    │
                    │  │   │ Micro SD    │   │    │
                    │  │   │ Card        │   │    │
                    │  │   └─────────────┘   │    │
                    │  │  1 2 3 4 5 6 7 8    │    │
                    │  └──┬─┬─┬─┬─┬─┬─┬─┬────┘    │
                    └─────┼─┼─┼─┼─┼─┼─┼─┼─────────┘
                          │ │ │ │ │ │ │ │
                         NC│ │ │ │ │ │ └─── CD (Card Detect)
                          CS│ │ │ │ └────── NC
                           MOSI│ │ └─────── SCK
                              GND└───────── 3V3
                                 MISO


    SD CARD PIN MAPPING (SPI Mode)
    ══════════════════════════════

    ┌──────┬──────────┬─────────┬─────────────────────────────┐
    │ Pin  │ SD Name  │ SPI     │ ESP32-S3 GPIO               │
    ├──────┼──────────┼─────────┼─────────────────────────────┤
    │  1   │ DAT2     │ NC      │ Not connected               │
    │  2   │ CD/DAT3  │ CS      │ GPIO 15 (active LOW)        │
    │  3   │ CMD      │ MOSI    │ GPIO 16                     │
    │  4   │ VDD      │ 3V3     │ 3.3V power                  │
    │  5   │ CLK      │ SCK     │ GPIO 17                     │
    │  6   │ VSS      │ GND     │ Ground                      │
    │  7   │ DAT0     │ MISO    │ GPIO 18                     │
    │  8   │ DAT1     │ NC      │ Not connected               │
    │  CD  │ Detect   │ CD      │ GPIO 8 (optional)           │
    └──────┴──────────┴─────────┴─────────────────────────────┘


    DETAILED CIRCUIT
    ════════════════

                                    3V3
                                     │
                                    ═══ 10µF (bulk decoupling)
                                     │
                                     ├───────────────────────────► SD VDD (Pin 4)
                                     │
                                    ═══ 100nF (close to SD slot)
                                     │
                                    GND


                      3V3           3V3           3V3           3V3
                       │             │             │             │
                      ┌┴┐           ┌┴┐           ┌┴┐           ┌┴┐
                  10K │ │       10K │ │       10K │ │       47K │ │
                      └┬┘           └┬┘           └┬┘           └┬┘
                       │             │             │             │
    GPIO 15 ───────────┼─────────────│─────────────│─────────────│──────► SD CS (Pin 2)
    (SD_CS)            │             │             │             │
                       │             │             │             │
    GPIO 16 ───────────│─────────────┼─────────────│─────────────│──────► SD MOSI (Pin 3)
    (SD_MOSI)          │             │             │             │
                       │             │             │             │
    GPIO 17 ───────────│─────────────│─────────────┼─────────────│──────► SD SCK (Pin 5)
    (SD_SCK)           │             │             │             │
                       │             │             │             │
    GPIO 18 ◄──────────│─────────────│─────────────│─────────────┼────── SD MISO (Pin 7)
    (SD_MISO)          │             │             │             │
                       │             │             │             │
                      Pull-ups ensure proper levels during card insertion

                                                                 3V3
                                                                  │
                                                                 ┌┴┐
                                                             47K │ │
                                                                 └┬┘
                                                                  │
    GPIO 8 ◄──────────────────────────────────────────────────────┼────── SD CD (Card Detect)
    (SD_CD)                                                       │
    (Optional)                                               ─┴─  │
                                                            ╱   ╲ │  Switch in SD slot
                                                           ─┬───┬─┘  (closes when card inserted)
                                                              │
                                                             GND

    Note: Card detect is optional - firmware can probe card presence via SPI


    SPI CONFIGURATION
    ═════════════════

    - SPI Frequency: 25 MHz (max for most SD cards in SPI mode)
    - SPI Mode: Mode 0 (CPOL=0, CPHA=0)
    - Initialization: Start at 400 kHz, switch to 25 MHz after init

    SD Card Library: Arduino SD.h or SdFat library


    FIRMWARE EXAMPLE
    ════════════════

    #define SD_CS_PIN   15
    #define SD_MOSI_PIN 16
    #define SD_SCK_PIN  17
    #define SD_MISO_PIN 18
    #define SD_CD_PIN   8   // Optional

    SPIClass sdSPI(HSPI);  // Use HSPI for SD card

    void setupSD() {
        sdSPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
        if (!SD.begin(SD_CS_PIN, sdSPI, 25000000)) {
            Serial.println("SD Card mount failed");
        }
    }
```

### Sheet 8: Expansion & Test Points

```
                              EXPANSION HEADER & TEST POINTS
════════════════════════════════════════════════════════════════════════════════════

    EXPANSION HEADER (J8) - Available GPIOs
    ═══════════════════════════════════════

    After SD card allocation, remaining available GPIOs:

                    ┌───────────────────────────────────────────────┐
                    │ IO0  IO1  IO38 IO39 IO40 IO41 IO42            │
                    │  ○    ○    ○    ○    ○    ○    ○              │
                    ├───────────────────────────────────────────────┤
                    │ IO45 IO46 IO47 IO48  3V3  GND  3V3  GND       │
                    │  ○    ○    ○    ○    ○    ○    ○    ○         │
                    └───────────────────────────────────────────────┘

    Notes:
    - IO0: Boot strapping pin - use with care (active low enters bootloader)
    - IO1: Available (USB_D- only if not using native USB)
    - IO45, IO46: Strapping pins - check boot mode requirements
    - GPIO 26-32: Reserved for flash (NOT available)
    - GPIO 33-37: Reserved for PSRAM (NOT available)
    - GPIO 15-18: Now used for SD card


    TEST POINTS
    ═══════════

    TP1 ────► 3V3 Rail (verify 3.3V ± 0.1V)

    TP2 ────► 12V Rail (verify 12V input)

    TP3 ────► RC Filter Output (0-3.3V DC, varies with PWM duty)

    TP4 ────► Op-Amp Output (0-5V, should be TP3 × 1.51)

    TP5 ────► MCP2515 INT (HIGH when idle, LOW on CAN activity)

    TP6 ────► EN (should be HIGH during normal operation)

    TP7 ────► GND (reference point)


```

### Sheet 9: Diagnostic Interfaces (JTAG & UART)

```
                              DIAGNOSTIC INTERFACES
════════════════════════════════════════════════════════════════════════════════════

    JTAG DEBUG HEADER (J9) - ARM Cortex 10-Pin (1.27mm pitch)
    ═══════════════════════════════════════════════════════════

    Standard ARM Cortex debug connector for JTAG/SWD debugging.
    Compatible with ESP-Prog, J-Link, and other JTAG adapters.

    ESP32-S3 JTAG Pins:
    - GPIO 39: MTCK (TCK) - Test Clock
    - GPIO 40: MTDO (TDO) - Test Data Out
    - GPIO 41: MTDI (TDI) - Test Data In
    - GPIO 42: MTMS (TMS) - Test Mode Select

                    ARM Cortex 10-Pin Header (J9)
                    1.27mm (0.05") pitch, 2×5 shrouded

                    ┌─────────────────────────┐
                    │ ┌───┬───┬───┬───┬───┐   │
                    │ │ 1 │ 3 │ 5 │ 7 │ 9 │   │  Pin 1 indicator
                    │ ├───┼───┼───┼───┼───┤   │  (notch/triangle)
                    │ │ 2 │ 4 │ 6 │ 8 │10 │   │
                    │ └───┴───┴───┴───┴───┘   │
                    └─────────────────────────┘

    Pin Assignment (ARM Cortex Debug Connector Standard):
    ┌─────┬────────┬─────────────────────────────────────────┐
    │ Pin │ Signal │ Description                             │
    ├─────┼────────┼─────────────────────────────────────────┤
    │  1  │ 3V3    │ Target voltage reference                │
    │  2  │ TMS    │ Test Mode Select (GPIO 42)              │
    │  3  │ GND    │ Ground                                  │
    │  4  │ TCK    │ Test Clock (GPIO 39)                    │
    │  5  │ GND    │ Ground                                  │
    │  6  │ TDO    │ Test Data Out (GPIO 40)                 │
    │  7  │ NC     │ Not connected (KEY position)            │
    │  8  │ TDI    │ Test Data In (GPIO 41)                  │
    │  9  │ GND    │ Ground                                  │
    │ 10  │ nRST   │ Reset (directly to EN pin)              │
    └─────┴────────┴─────────────────────────────────────────┘

    Mating Cable: Standard 10-pin 1.27mm IDC ribbon cable
    Compatible debuggers: ESP-Prog, Segger J-Link, FTDI C232HM


    JTAG ACTIVE INDICATOR (Optional)
    ════════════════════════════════

                      3V3
                       │
                      ┌┴┐
                  1K  │ │
                      └┬┘
                       │
                       ├──────────► LED (Debug Active)
                       │
                      ─┴─
                     ╱   ╲
                    ───────  from TMS or TCK
                       │
                      GND


    UART DEBUG HEADER (J10) - 6-Pin (2.54mm pitch)
    ══════════════════════════════════════════════

    Secondary serial port for diagnostics when USB is unavailable.
    Uses UART1 on available GPIOs (independent of USB CDC).

    ESP32-S3 UART Pins:
    - GPIO 43: U0TXD (default UART0 TX)
    - GPIO 44: U0RXD (default UART0 RX)

                    UART Header (J10)
                    2.54mm pitch, 1×6 right-angle

                    ┌───────────────────────────┐
                    │  1   2   3   4   5   6    │
                    │  ○   ○   ○   ○   ○   ○    │
                    └──┬───┬───┬───┬───┬───┬────┘
                       │   │   │   │   │   │
                      GND 3V3  TX  RX  EN  IO0

    Pin Assignment:
    ┌─────┬────────┬─────────────────────────────────────────┐
    │ Pin │ Signal │ Description                             │
    ├─────┼────────┼─────────────────────────────────────────┤
    │  1  │ GND    │ Ground                                  │
    │  2  │ 3V3    │ 3.3V output (for UART adapter)          │
    │  3  │ TX     │ UART TX (GPIO 43) → adapter RX          │
    │  4  │ RX     │ UART RX (GPIO 44) ← adapter TX          │
    │  5  │ EN     │ Reset (active LOW, directly to EN)      │
    │  6  │ IO0    │ Boot mode (LOW = bootloader)            │
    └─────┴────────┴─────────────────────────────────────────┘

    Compatible adapters: CP2102, CH340, FT232, any 3.3V USB-UART
    Baud rate: 115200 (default), up to 921600 supported

    IMPORTANT: Use 3.3V logic level adapters only!
               5V adapters will damage the ESP32-S3.


    AUTO-RESET CIRCUIT FOR UART PROGRAMMING
    ═══════════════════════════════════════

    Allows automatic bootloader entry from UART adapter DTR/RTS:

                          To UART Adapter
                              │    │
                             DTR  RTS
                              │    │
                             ═══  ═══
                            100nF 100nF
                              │    │
                              │    │      ┌─────────┐
                              │    └──────┤         │
                              │           │  NPN    ├───► EN
                              └───────────┤ 2N3904  │
                                          └────┬────┘
                                               │
                                              GND

                              │    │      ┌─────────┐
                              │    └──────┤         │
                              │           │  NPN    ├───► IO0
                              └───────────┤ 2N3904  │
                                          └────┬────┘
                                               │
                                              GND

    Note: This circuit is OPTIONAL if using native USB for programming.
          Only needed for UART-based programming/bootloader entry.
```

### Sheet 10: RPM Input (12V Square Wave Interface)

```
                              RPM INPUT INTERFACE
════════════════════════════════════════════════════════════════════════════════

    This circuit converts a 12V square wave signal (typically from an engine
    tachometer or ignition coil) to a 3.3V signal for the ESP32-S3 PCNT input.

    Features:
    - Galvanic isolation via optocoupler (protects ESP32 from automotive noise)
    - Reverse polarity protection
    - Input filtering via LED forward voltage drop
    - Clean digital output for hardware pulse counting


    RPM INPUT CONNECTOR (J11) - 2-Pin JST-PH
    ════════════════════════════════════════

                    ┌───────────┐
                    │  1     2  │
                    │  ○     ○  │
                    └──┬─────┬──┘
                       │     │
                      SIG   GND
                    (12V)  (0V)

    Pin Assignment:
    ┌─────┬────────┬─────────────────────────────────────────┐
    │ Pin │ Signal │ Description                             │
    ├─────┼────────┼─────────────────────────────────────────┤
    │  1  │ SIG    │ 12V square wave input (1 pulse/rev)     │
    │  2  │ GND    │ Signal ground (common with vehicle GND) │
    └─────┴────────┴─────────────────────────────────────────┘


    OPTOCOUPLER LEVEL SHIFTER CIRCUIT
    ══════════════════════════════════

         12V Square Wave Input (J11.1)
                │
                │
               ┌┴┐
               │ │ R20 = 2.2K (1/4W)
               │ │ Current limit: (12V - 1.2V) / 2.2K ≈ 4.9mA
               └┬┘
                │
                ├────────────┐
                │            │
                │           ─┴─
                │            ▲  D3 = 1N4148 (reverse protection)
                │           ─┬─
                │            │
                ▼ LED (pin 1)│
             ┌─────────┐     │
             │    ○────┼─────┘
             │  PC817  │
             │    ○────┼──────────────────────────────────────── GND (J11.2)
             │  (U5)   │
             │         │  Collector (pin 4)
             │    ○────┼──────┬──────────────────────────────── GPIO 1 (U1)
             │         │      │
             │    ○────┼──┐   │
             └─────────┘  │  ┌┴┐
                          │  │ │ R21 = 10K (pull-up)
                   Emitter│  │ │
                    (pin 3)  └┬┘
                          │   │
                         GND  └──────────────────────────────── 3V3


    PC817 OPTOCOUPLER PINOUT
    ════════════════════════

        ┌─────────────┐
        │  ┌───┐      │
        │  │LED│      │           Pin 1: Anode (LED +)
    1 ──┤  └─▼─┘      ├── 4       Pin 2: Cathode (LED -)
        │             │           Pin 3: Emitter (transistor)
    2 ──┤      ┌──┐   ├── 3       Pin 4: Collector (transistor)
        │      │▶ │   │
        │      └──┘   │
        └─────────────┘


    SIGNAL BEHAVIOR
    ════════════════

    Input (12V):     ┌────┐    ┌────┐    ┌────┐
                     │    │    │    │    │    │
              ───────┘    └────┘    └────┘    └────

    Output (GPIO):   ────┐    ┌────┐    ┌────┐    ┌────
                         │    │    │    │    │    │
                         └────┘    └────┘    └────┘

    Note: Output is INVERTED due to common-emitter configuration.
          The ESP32 PCNT is configured to count FALLING edges.


    DESIGN CALCULATIONS
    ═══════════════════

    1. LED Current (nominal 12V input):
       I_LED = (V_IN - V_LED) / R20
             = (12V - 1.2V) / 2.2kΩ
             = 4.9 mA
       PC817 rated for 50mA max, 4.9mA provides good margin.

    2. LED Current (worst case 14.4V input during charging):
       I_LED = (14.4V - 1.2V) / 2.2kΩ
             = 6.0 mA
       Still well within limits.

    3. LED Current (low battery 10V):
       I_LED = (10V - 1.2V) / 2.2kΩ
             = 4.0 mA
       Still sufficient for reliable operation (PC817 needs ~1mA min).

    4. Maximum Frequency:
       PC817 typical rise/fall time: 4µs/3µs
       Maximum reliable frequency: ~50 kHz
       At 10,000 RPM with 1 pulse/rev: 167 Hz (well within limits)

    5. Pull-up Resistor:
       R21 = 10kΩ provides ~0.33mA collector current when ON.
       PC817 CTR (Current Transfer Ratio) typically 50-300%.
       With 4.9mA LED current, transistor can sink up to 14.7mA.
       10kΩ pull-up requires only 0.33mA - very reliable.


    ALTERNATIVE: TRANSISTOR LEVEL SHIFTER (No Isolation)
    ════════════════════════════════════════════════════

    If galvanic isolation is not required, a simpler circuit can be used:

         12V Square Wave Input
                │
               ┌┴┐
               │ │ 10K
               └┬┘
                │
                ├──────┬──────────────────── GPIO 1
                │      │
               ┌┴┐    ┌┴┐
          22K  │ │    │ │ 10K (pull-up to 3.3V)
               └┬┘    └┬┘
                │      │
                │      └───────────────────── 3V3
               ─┴─
              ╱   ╲  3.3V Zener (protection)
               ───
                │
               GND

    This circuit is simpler but does NOT provide isolation.
    Use optocoupler version for automotive applications.


    WIRING NOTES
    ════════════

    1. Source Signals:
       - Ignition coil negative terminal (inverted tach signal)
       - ECU tachometer output (if available)
       - Aftermarket tach adapter output

    2. Wire Gauge:
       - 22-24 AWG sufficient for signal level
       - Keep wires twisted pair for noise rejection

    3. Shielding:
       - Use shielded cable in high-noise environments
       - Connect shield to GND at one end only

    4. Ground Connection:
       - J11.2 must share common ground with signal source
       - Optocoupler provides isolation between vehicle and ESP32 grounds
```

### Sheet 11: VSS Input (Vehicle Speed Sensor - GM 700R4)

```
                              VSS INPUT INTERFACE (LM1815 VR Sensor)
════════════════════════════════════════════════════════════════════════════════

    This circuit converts the AC sine wave signal from a GM 700R4 transmission
    Variable Reluctance (VR) speed sensor to a 3.3V digital signal for the
    ESP32-S3 PCNT input.

    Features:
    - LM1815 dedicated VR sensor interface IC
    - Adaptive threshold automatically adjusts to signal amplitude
    - Works reliably from parking lot speeds to highway speeds
    - Minimal external components
    - 8000 PPM (Pulses Per Mile) sensor compatibility


    VSS INPUT CONNECTOR (J12) - 2-Pin JST-PH
    ════════════════════════════════════════

                    ┌───────────┐
                    │  1     2  │
                    │  ○     ○  │
                    └──┬─────┬──┘
                       │     │
                     VR+    VR-
                   (Signal) (GND)

    Pin Assignment:
    ┌─────┬────────┬─────────────────────────────────────────┐
    │ Pin │ Signal │ Description                             │
    ├─────┼────────┼─────────────────────────────────────────┤
    │  1  │ VR+    │ VR sensor signal (AC sine wave)         │
    │  2  │ VR-    │ Sensor ground (transmission case GND)   │
    └─────┴────────┴─────────────────────────────────────────┘


    LM1815 VR SENSOR INTERFACE CIRCUIT
    ══════════════════════════════════

                                    3V3
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
                                           │ 7 OUTPUT ─────────────┼──┼── GPIO 38 (U1)
                                           │                        │  │
                       ┌───────────────────┤ 6 RSET                │  │
                       │                   │                        │  │
                      ┌┴┐                  │ 5 CSET (NC)           │  │
                  R23 │ │                  │                        │  │
                 200K │ │                  │ 4 VR-  ────────────────┼──┼── GND (J12.2)
                      └┬┘                  │                        │  │
                       │                   │ 3 VR+  ───┬────────────┼──┼── VR Signal (J12.1)
                      ─┴─                  │           │            │  │
                      ///                  │ 2 NC      │            │  │
                                           │           │            │  │
                                           │ 1 GND ────┼────────────┼──┘
                                           │           │            │
                                           └───────────┼────────────┘
                                                       │
                                                      ═══ C15
                                                      │   1nF (optional filter)
                                                      │
                                                     ─┴─
                                                     ///
                                                     GND


                                    TVS Diode (Transient Protection)
                                    ════════════════════════════════

                     VR Signal (J12.1)
                            │
                       ┌────┴────┐
                       │  TVS    │
                       │P6KE33CA │
                       │(Bi-dir) │
                       └────┬────┘
                            │
                           ─┴─
                           ///
                           GND


    LM1815 PINOUT REFERENCE
    ═══════════════════════

            ┌─────────────────┐
            │  ┌───────────┐  │
        1 ──┤  │           │  ├── 8   Pin 1: GND
            │  │  LM1815   │  │       Pin 2: NC (No Connection)
        2 ──┤  │           │  ├── 7   Pin 3: VR+ (Sensor Input +)
            │  │           │  │       Pin 4: VR- (Sensor Input -)
        3 ──┤  │           │  ├── 6   Pin 5: CSET (Internal - NC)
            │  │           │  │       Pin 6: RSET (Threshold Timing)
        4 ──┤  │           │  ├── 5   Pin 7: OUTPUT (Open Collector)
            │  └───────────┘  │       Pin 8: VCC (3.3V-12V)
            └─────────────────┘


    DESIGN CALCULATIONS
    ═══════════════════

    1. RSET Selection (R23):
       200kΩ provides good general-purpose adaptive threshold behavior
       - Lower values (50k-100k): Faster adaptation, less noise immunity
       - Higher values (200k-500k): Slower adaptation, better noise immunity

    2. Output Pull-up (R22):
       I_pullup = 3.3V / 10kΩ = 0.33 mA
       LM1815 output can sink up to 20mA
       0.33mA << 20mA  ✓ Reliable operation guaranteed

    3. Expected Frequency Range (8000 PPM):
       At 5 MPH:   11 Hz
       At 60 MPH:  133 Hz
       At 120 MPH: 267 Hz
       LM1815 supports up to 30kHz+ - well within limits

    4. Speed Calculation:
       Speed (MPH) = (Frequency_Hz × 3600) / 8000
                   = Frequency_Hz × 0.45


    SIGNAL BEHAVIOR
    ═══════════════

    VR Sensor Input:     /\      /\      /\      /\
    (AC Sine)           /  \    /  \    /  \    /  \
                       /    \  /    \  /    \  /    \
                  ────/      \/      \/      \/      \────

    LM1815 Output:   ┌────┐  ┌────┐  ┌────┐  ┌────┐
    (Digital)        │    │  │    │  │    │  │    │
                  ───┘    └──┘    └──┘    └──┘    └────

    ESP32 PCNT counts edges (configurable: rising or falling)


    WIRING NOTES
    ════════════

    1. Sensor Location:
       - GM 700R4 VSS sensor is mounted on the tail housing
       - 2-wire connector (signal + ground)

    2. Cable Requirements:
       - 20-22 AWG twisted pair (shielded preferred)
       - Maximum length: 5 meters
       - Shield connects to GND at board end only

    3. Ground Connection:
       - VR- (J12.2) connects to transmission case ground
       - Ensure good chassis ground at transmission

    4. Routing:
       - Keep away from ignition wires and spark plug cables
       - Use grommets through firewalls
       - Leave service loop near transmission
```

### Sheet 12: MCP23017 I2C GPIO Expander & Encoder Array

```
                              MCP23017 ENCODER MULTIPLEXER
════════════════════════════════════════════════════════════════════════════════

    The MCP23017 provides 16 GPIO pins via I2C, allowing up to 5 rotary encoders
    (3 pins each: CLK, DT, SW) using only 2 ESP32 pins.

    Features:
    - 5 rotary encoder connectors (J13-J17)
    - Single I2C bus (GPIO 47/48)
    - Optional interrupt for responsive detection
    - J13 assigned to Power Steering speed control


    MCP23017 I2C GPIO EXPANDER CIRCUIT (U7)
    ═══════════════════════════════════════

                                      3.3V
                                       │
          ┌────────────────────────────┼────────────────────────────────┐
          │                            │                                │
         ═══ C16                      ┌┴┐                              ┌┴┐
         │   100nF                R24 │ │ 10K                      R25 │ │ 10K
         │                            └┬┘                              └┬┘
        ─┴─                            │                                │
        ///                            │                                │
                                       │                                │
    GPIO 47 (SDA) ─────────────────────┴────────────────────────────────┼────┐
                                                                        │    │
    GPIO 48 (SCL) ──────────────────────────────────────────────────────┘    │
                                                                              │
                                       ┌──────────────────────────────────────┘
                                       │
                                       │
                         ┌─────────────┴─────────────────────────────────────┐
                         │                  MCP23017 (U7)                    │
                         │                                                   │
    3.3V ────────────────┤ 9  VDD                                           │
                         │                                                   │
    GND ─────────────────┤ 10 VSS                                           │
                         │                                                   │
    GPIO 47 (SDA) ───────┤ 13 SDA                                           │
                         │                                                   │
    GPIO 48 (SCL) ───────┤ 12 SCL                                           │
                         │                                                   │
    GND ─────────────────┤ 15 A0 ─┐                                         │
    GND ─────────────────┤ 16 A1 ─┼── Address = 0x20                        │
    GND ─────────────────┤ 17 A2 ─┘                                         │
                         │                                                   │
    3.3V ────────────────┤ 18 RESET (active LOW, tie HIGH)                  │
                         │                                                   │
    GPIO 45 ◄────────────┤ 20 INTA (optional interrupt)                     │
                         │                                                   │
                         │                    DIRECTLY TO ENCODER CONNECTORS │
                         │                                                   │
                         │ 21 GPA0 ────────────────────────────► J13 Pin 1 (ENC1_CLK) │
                         │ 22 GPA1 ────────────────────────────► J13 Pin 2 (ENC1_DT)  │
                         │ 23 GPA2 ────────────────────────────► J13 Pin 3 (ENC1_SW)  │
                         │                                                   │
                         │ 24 GPA3 ────────────────────────────► J14 Pin 1 (ENC2_CLK) │
                         │ 25 GPA4 ────────────────────────────► J14 Pin 2 (ENC2_DT)  │
                         │ 26 GPA5 ────────────────────────────► J14 Pin 3 (ENC2_SW)  │
                         │                                                   │
                         │ 27 GPA6 ────────────────────────────► J15 Pin 1 (ENC3_CLK) │
                         │ 28 GPA7 ────────────────────────────► J15 Pin 2 (ENC3_DT)  │
                         │ 1  GPB0 ────────────────────────────► J15 Pin 3 (ENC3_SW)  │
                         │                                                   │
                         │ 2  GPB1 ────────────────────────────► J16 Pin 1 (ENC4_CLK) │
                         │ 3  GPB2 ────────────────────────────► J16 Pin 2 (ENC4_DT)  │
                         │ 4  GPB3 ────────────────────────────► J16 Pin 3 (ENC4_SW)  │
                         │                                                   │
                         │ 5  GPB4 ────────────────────────────► J17 Pin 1 (ENC5_CLK) │
                         │ 6  GPB5 ────────────────────────────► J17 Pin 2 (ENC5_DT)  │
                         │ 7  GPB6 ────────────────────────────► J17 Pin 3 (ENC5_SW)  │
                         │                                                   │
                         │ 8  GPB7 ──── NC (available for future use)       │
                         │                                                   │
                         └───────────────────────────────────────────────────┘


    ENCODER CONNECTORS (J13-J17) - JST-XH 5-Pin
    ═══════════════════════════════════════════

    All encoder connectors share the same pinout:

                    ┌───────────────────────────┐
                    │ ┌───┬───┬───┬───┬───┐     │
                    │ │ 1 │ 2 │ 3 │ 4 │ 5 │     │  ← Polarization key
                    │ └─┬─┴─┬─┴─┬─┴─┬─┴─┬─┘     │
                    └───┼───┼───┼───┼───┼───────┘
                        │   │   │   │   │
                       CLK  DT  SW  3V3 GND
                       (A)  (B)

    Pin Assignment (same for J13, J14, J15, J16, J17):
    ┌─────┬────────┬─────────────────────────────────────┐
    │ Pin │ Signal │ Description                         │
    ├─────┼────────┼─────────────────────────────────────┤
    │  1  │ CLK    │ Encoder Channel A (via MCP23017)    │
    │  2  │ DT     │ Encoder Channel B (via MCP23017)    │
    │  3  │ SW     │ Push button (via MCP23017)          │
    │  4  │ 3V3    │ 3.3V power for encoder              │
    │  5  │ GND    │ Ground                              │
    └─────┴────────┴─────────────────────────────────────┘

    Mating Connector: JST XHP-5 (housing) + SXH-001T-P0.6 (crimp terminals)


    ENCODER CONNECTOR ASSIGNMENTS
    ═════════════════════════════

    ┌──────┬────────────────────────────┬───────────────────────────────┐
    │ Conn │ Function                   │ MCP23017 Pins                 │
    ├──────┼────────────────────────────┼───────────────────────────────┤
    │ J13  │ Power Steering Speed       │ GPA0 (CLK), GPA1 (DT), GPA2 (SW) │
    │ J14  │ Future Use                 │ GPA3 (CLK), GPA4 (DT), GPA5 (SW) │
    │ J15  │ Future Use                 │ GPA6 (CLK), GPA7 (DT), GPB0 (SW) │
    │ J16  │ Future Use                 │ GPB1 (CLK), GPB2 (DT), GPB3 (SW) │
    │ J17  │ Future Use                 │ GPB4 (CLK), GPB5 (DT), GPB6 (SW) │
    └──────┴────────────────────────────┴───────────────────────────────┘


    MCP23017 PINOUT REFERENCE (DIP-28 / SOIC-28)
    ════════════════════════════════════════════

              ┌─────────────────────────────┐
              │  ┌───────────────────────┐  │
      GPB0 ───┤1                       28├─── GPA7
      GPB1 ───┤2                       27├─── GPA6
      GPB2 ───┤3                       26├─── GPA5
      GPB3 ───┤4                       25├─── GPA4
      GPB4 ───┤5       MCP23017        24├─── GPA3
      GPB5 ───┤6                       23├─── GPA2
      GPB6 ───┤7                       22├─── GPA1
      GPB7 ───┤8                       21├─── GPA0
       VDD ───┤9                       20├─── INTA
       VSS ───┤10                      19├─── INTB
        NC ───┤11                      18├─── RESET
       SCL ───┤12                      17├─── A2
       SDA ───┤13                      16├─── A1
        NC ───┤14                      15├─── A0
              │  └───────────────────────┘  │
              └─────────────────────────────┘


    INTERNAL PULL-UPS
    ═════════════════

    The MCP23017 has configurable internal pull-ups (100kΩ typ).
    Enable them in firmware for encoder inputs:

        mcp.pinMode(pin, INPUT_PULLUP);

    This eliminates the need for external pull-up resistors on encoder lines.


    POWER STEERING SPEED CONTROL (J13 / ENC1)
    ═════════════════════════════════════════

    The first encoder (J13) is dedicated to controlling the power steering
    assist motor speed via the PWM output (J5).

    Control Flow:
    ┌─────────────┐      ┌───────────┐      ┌──────────┐      ┌────────────┐
    │   Rotary    │      │ MCP23017  │      │  ESP32   │      │  PWM Out   │
    │  Encoder    │─────►│   I2C     │─────►│ Process  │─────►│   (J5)     │
    │   (J13)     │      │   (U7)    │      │  Speed   │      │  0-5V      │
    └─────────────┘      └───────────┘      └──────────┘      └────────────┘
                                                                    │
                                                                    ▼
                                                          ┌─────────────────┐
                                                          │  Power Steering │
                                                          │   Motor Driver  │
                                                          └─────────────────┘

    Settings (defined in config.h):
    - Range: 0-100%
    - Step: 5% per detent
    - Default: 50%


    WIRING NOTES
    ════════════

    1. I2C Bus:
       - Keep I2C traces short (<10cm on PCB)
       - 10K pull-ups on SDA/SCL (R24, R25)
       - Decoupling cap (C16) close to MCP23017

    2. Encoder Cables:
       - 24-28 AWG stranded wire
       - Keep cables under 50cm
       - Shielded cable recommended for panel-mount encoders

    3. Power:
       - 3.3V provided on each connector
       - Total encoder current: ~5mA each = 25mA max for all 5
```

### Sheet 13: Water Temperature Input (LS1 Coolant Sensor)

```
                              WATER TEMPERATURE SENSOR INTERFACE
════════════════════════════════════════════════════════════════════════════════

    This circuit interfaces a GM LS1-style NTC thermistor coolant temperature
    sensor with the ESP32-S3 ADC input for engine temperature monitoring.

    Features:
    - 1kΩ pull-up resistor for optimal ADC range in operating temps
    - RC low-pass filter removes ignition noise
    - ESD protection via Zener clamp
    - Compatible with LS1, LS2, LS3, LS6, LS7, LT1, and similar GM sensors


    WATER TEMP INPUT CONNECTOR (J18) - 2-Pin JST-PH
    ════════════════════════════════════════════════

                    ┌───────────┐
                    │  1     2  │
                    │  ○     ○  │
                    └──┬─────┬──┘
                       │     │
                      SIG   GND
                   (Sensor) (Chassis)

    Pin Assignment:
    ┌─────┬────────┬─────────────────────────────────────────────┐
    │ Pin │ Signal │ Description                                 │
    ├─────┼────────┼─────────────────────────────────────────────┤
    │  1  │ SIG    │ Sensor signal wire (to CTS sensor)          │
    │  2  │ GND    │ Chassis ground (to engine block)            │
    └─────┴────────┴─────────────────────────────────────────────┘

    Mating Connector: JST PHR-2 (housing) + SPH-002T-P0.5S (crimp terminals)


    SENSOR SPECIFICATIONS (GM LS1 CTS - Part# 12146312)
    ════════════════════════════════════════════════════

    Type: NTC (Negative Temperature Coefficient) Thermistor
    Configuration: Single-wire, grounded through engine block
    Thread: 3/8" NPT

    Resistance vs Temperature:
    ┌────────────────┬────────────────┬────────────────────────────┐
    │ Temperature    │ Resistance     │ ADC Output (12-bit, 1kΩ)   │
    ├────────────────┼────────────────┼────────────────────────────┤
    │ 70°F (21°C)    │ 3,400 Ω        │ 2.55V → 3161               │
    │ 140°F (60°C)   │ 667 Ω          │ 1.32V → 1635               │
    │ 195°F (90°C)   │ 200 Ω          │ 0.55V → 681                │
    │ 220°F (104°C)  │ 120 Ω          │ 0.35V → 434                │
    │ 230°F (110°C)  │ 100 Ω          │ 0.30V → 372                │
    │ Open circuit   │ ∞              │ 3.3V → 4095                │
    │ Short circuit  │ 0 Ω            │ 0V → 0                     │
    └────────────────┴────────────────┴────────────────────────────┘


    COMPLETE CIRCUIT SCHEMATIC
    ══════════════════════════

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
                    │                          ─────  BZT52C3V3 (3.3V Zener)│
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


    CIRCUIT DESCRIPTION
    ═══════════════════

    1. PULL-UP RESISTOR (R27 = 1kΩ):
       - Creates voltage divider with NTC sensor
       - 3.3V reference compatible with ESP32 ADC (no 5V needed)
       - 1kΩ provides good resolution in 100-500Ω operating range
       - Vout = 3.3V × R_sensor / (1000 + R_sensor)

    2. FILTER CAPACITOR (C17 = 100nF):
       - Filters high-frequency noise from engine electrical system
       - Placed close to pull-up for best filtering
       - Does not significantly affect DC voltage reading

    3. ESD PROTECTION (R28 + D5):
       - R28 (10kΩ) limits current during ESD events
       - D5 (3.3V Zener BZT52C3V3) clamps voltage to safe level
       - Protects GPIO from ignition noise and static discharge

    4. LOW-PASS FILTER (R29 + C18):
       - RC filter with fc = 1/(2π × 1000 × 0.0000001) ≈ 1.6kHz
       - Removes high-frequency noise while preserving slow temp changes
       - Temperature changes occur over seconds - 1.6kHz is conservative


    DESIGN CALCULATIONS
    ═══════════════════

    1. Pull-up Current at max temp (70Ω sensor @ 266°F):
       I_max = 3.3V / (1000Ω + 70Ω) = 3.1 mA
       Well within sensor and resistor ratings.

    2. Power Dissipation in R27:
       P = I² × R = (0.0031)² × 1000 = 9.6 mW
       1/4W (250mW) resistor has large safety margin.

    3. ADC Resolution at operating temp (200Ω @ 195°F):
       Vout = 3.3V × 200 / (1000 + 200) = 0.55V
       ADC = 0.55V / 3.3V × 4095 = 681 counts
       Change per 1°F near operating temp: ~15 ADC counts
       Excellent resolution for temperature monitoring.

    4. RC Filter Cutoff (R29, C18):
       fc = 1 / (2π × 1000 × 100nF) = 1592 Hz
       Removes ignition noise (typically >1kHz) effectively.


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


    WIRING NOTES
    ════════════

    1. Sensor Location:
       - LS1 has two CTS sensors - use the one for the gauge cluster
       - Located on driver's side cylinder head (front)
       - Single-wire sensor - case grounds through engine block

    2. Wire Gauge:
       - 18-22 AWG is sufficient for signal level
       - Keep wire run under 3 meters if possible

    3. Shielding:
       - Shielded cable recommended for noise immunity
       - Connect shield to GND at board end only (single-point ground)

    4. Ground Connection:
       - J18.2 should connect to engine block ground
       - Ensure good chassis ground path for accurate readings

    5. Heat Exposure:
       - Wire should be rated for engine bay temperatures
       - Use high-temp silicone or PTFE insulation wire (150°C+)
```


    DEBUG USE CASES
    ═══════════════

    1. DEVELOPMENT (JTAG)
       - Step-through debugging with breakpoints
       - Real-time variable inspection
       - Flash programming via OpenOCD
       - Core dump analysis

    2. PRODUCTION TESTING (UART)
       - Serial console for test scripts
       - Log output during manufacturing test
       - Firmware update without USB

    3. FIELD DIAGNOSTICS (UART)
       - Connect laptop for live debugging
       - Read error logs
       - Manual command input

    4. BOOTLOADER RECOVERY (UART + IO0)
       - Hold IO0 LOW during reset to enter bootloader
       - Flash firmware when main app is corrupted
       - Works even if USB CDC firmware is broken
```

## Complete Pin Assignment Table

| GPIO | Function | Direction | Connected To | Notes |
|------|----------|-----------|--------------|-------|
| 0 | BOOT | I | Boot button / J10 Pin 6 | Strapping pin (LOW=bootloader) |
| 1 | RPM_IN | I | Optocoupler output (U5) | 12V square wave via PC817 |
| 2 | COMM_SPI_MOSI | O | Slave MOSI (J6) | SPI to slave |
| 3 | COMM_SPI_MISO | I | Slave MISO (J6) | SPI from slave |
| 4 | WATER_TEMP_IN | I | Water temp sensor (J18) | ADC1_CH3, NTC thermistor |
| 5 | - | - | Expansion | Available (was encoder) |
| 6 | - | - | Expansion | Available (was encoder) |
| 7 | PWM_OUT | O | RC Filter (R1) | PWM motor control |
| 8 | SD_CD | I | SD Card Detect (J7) | Optional card detect |
| 9 | MCP_INT | I | MCP2515 INT (J3) | CAN interrupt |
| 10 | MCP_CS | O | MCP2515 CS (J3) | CAN chip select |
| 11 | MCP_MOSI | O | MCP2515 SI (J3) | CAN SPI data out |
| 12 | MCP_SCK | O | MCP2515 SCK (J3) | CAN SPI clock |
| 13 | MCP_MISO | I | MCP2515 SO (J3) | CAN SPI data in |
| 14 | COMM_SPI_SCK | O | Slave SCK (J6) | SPI clock to slave |
| 15 | SD_CS | O | SD Card CS (J7) | SD card chip select |
| 16 | SD_MOSI | O | SD Card MOSI (J7) | SD card data out |
| 17 | SD_SCK | O | SD Card SCK (J7) | SD card clock |
| 18 | SD_MISO | I | SD Card MISO (J7) | SD card data in |
| 19 | USB_D- | I/O | USB-C D- | Native USB |
| 20 | USB_D+ | I/O | USB-C D+ | Native USB |
| 21 | COMM_SPI_CS | O | Slave CS (J6) | SPI chip select |
| 22-25 | - | - | Expansion | Available |
| 26-32 | FLASH | - | Internal | Reserved - DO NOT USE |
| 33-37 | PSRAM | - | Internal | Reserved - DO NOT USE |
| 38 | VSS_IN | I | LM1815 output (U6) | VR speed sensor via J12 |
| 39 | JTAG_TCK | I/O | J9 Pin 4 | JTAG Test Clock |
| 40 | JTAG_TDO | O | J9 Pin 6 | JTAG Test Data Out |
| 41 | JTAG_TDI | I | J9 Pin 8 | JTAG Test Data In |
| 42 | JTAG_TMS | I/O | J9 Pin 2 | JTAG Test Mode Select |
| 43 | UART_TX | O | J10 Pin 3 | Debug UART transmit |
| 44 | UART_RX | I | J10 Pin 4 | Debug UART receive |
| 45 | MCP23017_INT | I | MCP23017 INTA (U7) | Encoder interrupt (optional) |
| 46 | - | - | Expansion | Available |
| 47 | I2C_SDA | I/O | MCP23017 SDA (U7) | I2C data for encoders |
| 48 | I2C_SCL | O | MCP23017 SCL (U7) | I2C clock for encoders |
| EN | Reset | I | Reset btn / J9 Pin 10 / J10 Pin 5 | Active low reset |

## Bill of Materials (BOM)

### Integrated Circuits

| Ref | Part | Package | Qty | Notes |
|-----|------|---------|-----|-------|
| U1 | ESP32-S3-WROOM-1-N16R8 | Module | 1 | 16MB Flash, 8MB PSRAM |
| U2 | MP2359DJ-LF-Z | SOT23-6 | 1 | Buck regulator (or MP1584 module) |
| U3 | LM358P | DIP-8/SOIC-8 | 1 | Dual op-amp |
| U4 | USBLC6-2SC6 | SOT23-6 | 1 | USB ESD protection |
| U5 | PC817C | DIP-4/SOP-4 | 1 | Optocoupler (RPM input isolation) |
| U6 | LM1815M | SOIC-8 | 1 | VR sensor interface (VSS input) |
| U7 | MCP23017 | DIP-28/SOIC-28 | 1 | I2C GPIO expander (5 encoders) |

### Passive Components

| Ref | Value | Package | Qty | Notes |
|-----|-------|---------|-----|-------|
| C1 | 22µF/25V | 0805/Radial | 1 | Buck input |
| C2 | 22µF/10V | 0805/Radial | 1 | Buck output |
| C3-C6 | 100nF | 0402/0603 | 6 | Decoupling (ESP32, SD card) |
| C7 | 10µF | 0805 | 2 | Bulk decoupling (ESP32, SD) |
| C8 | 1µF | 0805 | 1 | RC filter |
| C9 | 10µF | 0805 | 1 | USB VBUS |
| L1 | 10µH | 4x4mm | 1 | Buck inductor |
| R1 | 33K | 0402/0603 | 1 | FB divider high |
| R2 | 10K | 0402/0603 | 6 | FB divider, gain, SD pull-ups |
| R3 | 10K | 0402/0603 | 1 | RC filter |
| R4 | 5.1K | 0402/0603 | 3 | Feedback, USB CC |
| R5-R6 | 10K | 0402/0603 | 2 | Pull-ups (EN, IO0) |
| R8-R9 | 47K | 0402/0603 | 2 | SD MISO, Card detect pull-up |
| R20 | 2.2K | 0402/0603 | 1 | RPM optocoupler LED current limit |
| R21 | 10K | 0402/0603 | 1 | RPM optocoupler collector pull-up |
| R22 | 10K | 0402/0603 | 1 | VSS LM1815 output pull-up |
| R23 | 200K | 0402/0603 | 1 | VSS LM1815 RSET (threshold timing) |
| C14 | 100nF | 0402 | 1 | VSS LM1815 VCC bypass |
| C15 | 1nF | 0402 | 1 | VSS input filter (optional) |
| R24 | 10K | 0402/0603 | 1 | I2C SDA pull-up (MCP23017) |
| R25 | 10K | 0402/0603 | 1 | I2C SCL pull-up (MCP23017) |
| R26 | 10K | 0402/0603 | 1 | SPI slave CS pull-up (boot protection) |
| R27 | 1K 1% | 0402/0603 | 1 | Water temp pull-up resistor |
| R28 | 10K | 0402/0603 | 1 | Water temp ESD current limit |
| R29 | 1K | 0402/0603 | 1 | Water temp RC filter resistor |
| C16 | 100nF | 0402 | 1 | MCP23017 VCC bypass |
| C17 | 100nF | 0402 | 1 | Water temp pull-up filter |
| C18 | 100nF | 0402 | 1 | Water temp RC filter capacitor |

### Connectors (Locking Cable Connectors)

| Ref | Type | Part Number | Pins | Notes |
|-----|------|-------------|------|-------|
| J1 | Screw Terminal 5.08mm | Phoenix 1935161 | 2 | 12V power input (up to 10A) |
| J2 | USB-C Receptacle | GCT USB4110 | 16 | USB programming/debug |
| J3 | JST-XH Vertical | B7B-XH-A | 7 | MCP2515 CAN module |
| J4 | JST-GH Vertical | SM04B-GHS-TB | 4 | CAN bus cable (CANH/CANL/12V/GND) |
| J5 | JST-PH Vertical | B3B-PH-K-S | 3 | PWM output (SIG/5V/GND) |
| J6 | JST-GH Vertical | SM06B-GHS-TB | 6 | SPI slave cable |
| J7 | Micro SD Slot | Molex 504077 | 8+1 | Push-push micro SD |
| J8 | Pin Header 2.54mm | - | 2×7 | Expansion GPIOs |
| J9 | ARM Cortex Debug 1.27mm | Samtec FTSH-105-01 | 2×5 | JTAG/SWD debug (shrouded) |
| J10 | Pin Header 2.54mm RA | - | 1×6 | UART debug (right-angle) |
| J11 | JST-PH Vertical | B2B-PH-K-S | 2 | RPM input (12V square wave) |
| J12 | JST-PH Vertical | B2B-PH-K-S | 2 | VSS input (700R4 VR sensor) |
| J13 | JST-XH Vertical | B5B-XH-A | 5 | Encoder 1 - Power Steering (via MCP23017) |
| J14 | JST-XH Vertical | B5B-XH-A | 5 | Encoder 2 - Future Use (via MCP23017) |
| J15 | JST-XH Vertical | B5B-XH-A | 5 | Encoder 3 - Future Use (via MCP23017) |
| J16 | JST-XH Vertical | B5B-XH-A | 5 | Encoder 4 - Future Use (via MCP23017) |
| J17 | JST-XH Vertical | B5B-XH-A | 5 | Encoder 5 - Future Use (via MCP23017) |
| J18 | JST-PH Vertical | B2B-PH-K-S | 2 | Water Temp (LS1 CTS sensor) |

### Mating Cable Connectors (for harness assembly)

| For | Housing | Crimp Terminal | Notes |
|-----|---------|----------------|-------|
| J3 (MCP2515) | XHP-7 | SXH-001T-P0.6 | JST-XH 7-pos |
| J4 (CAN bus) | GHR-04V-S | SSH-003T-P0.2 | JST-GH 4-pos |
| J5 (PWM out) | PHR-3 | SPH-002T-P0.5S | JST-PH 3-pos |
| J6 (SPI slave) | GHR-06V-S | SSH-003T-P0.2 | JST-GH 6-pos |
| J11 (RPM in) | PHR-2 | SPH-002T-P0.5S | JST-PH 2-pos |
| J12 (VSS in) | PHR-2 | SPH-002T-P0.5S | JST-PH 2-pos |
| J13-J17 (Encoders) | XHP-5 | SXH-001T-P0.6 | JST-XH 5-pos (×5) |
| J18 (Water temp) | PHR-2 | SPH-002T-P0.5S | JST-PH 2-pos |

### Miscellaneous

| Ref | Part | Qty | Notes |
|-----|------|-----|-------|
| SW1 | Tactile Switch 6mm | 1 | Reset button |
| SW2 | Tactile Switch 6mm | 1 | Boot button (optional) |
| D1 | SS34 | 1 | Reverse polarity protection |
| D2 | SMBJ15A | 1 | TVS surge protection |
| D3 | 1N4148 | 1 | RPM input reverse protection |
| D4 | P6KE33CA | 1 | VSS transient protection (bidirectional) |
| D5 | BZT52C3V3 | 1 | Water temp ESD/overvoltage clamp (3.3V Zener) |
| JP1 | 2-pin header + jumper | 1 | CAN 120Ω termination select |

## PCB Design Notes

### Layer Stack (2-layer recommended minimum)

```
┌──────────────────────────────┐
│       Top Layer (Signal)     │  Components, signal traces
├──────────────────────────────┤
│     FR4 Core (1.6mm)         │
├──────────────────────────────┤
│    Bottom Layer (GND/Power)  │  Ground plane, power distribution
└──────────────────────────────┘
```

### Critical Layout Rules

1. **Power Supply**
   - Keep buck converter components close together
   - Short, wide traces for high-current paths
   - Input/output capacitors directly at IC pins

2. **ESP32-S3 Module**
   - Keep antenna area clear of copper (both layers)
   - Decoupling caps within 3mm of power pins
   - No traces under module if possible

3. **USB**
   - Match D+/D- trace lengths (±0.5mm)
   - 90Ω differential impedance for USB pair
   - Keep USB traces away from high-speed signals

4. **CAN Bus**
   - CANH/CANL as differential pair
   - 120Ω termination close to connector if used

5. **SPI Signals**
   - Keep clock traces short
   - Ground guard traces between SPI buses if both active
```

### Board Dimensions

Optimized size: 80mm × 60mm (improved thermal management, signal routing, and connector accessibility)

### Optimized Component Placement Layout

Design principles applied:
- ESP32-S3 antenna at board edge with keepout zone
- USB traces minimized (<15mm from module to connector)
- Power section isolated with thermal relief
- Analog circuits separated from digital noise
- Vehicle interface connectors grouped on right edge
- Debug interfaces accessible during development
- Related connectors grouped for cable management

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  ○                              TOP EDGE                                      ○  │
│                                                                                  │
│   ╔═══════════════╗                              ░░░░░░░░░░░░░░░░░░░░░░░░░░░    │
│   ║  POWER ZONE   ║                              ░░░░░░░░░░░░░░░░░░░░░░░░░░░    │
│   ║ ┌───────────┐ ║     ┌─────────────────────┐  ░░░  ANTENNA KEEPOUT    ░░░    │
│   ║ │ 12V INPUT │ ║     │  ESP32-S3-WROOM-1   │  ░░░  NO COPPER/TRACES   ░░░    │
│   ║ │   (J1)    │ ║     │    ┌──────────┐     │  ░░░░░░░░░░░░░░░░░░░░░░░░░░░    │
│   ║ │  SCREW    │ ║     │    │ ANTENNA  │─────┼──░░░░░░░░░░░░░░░░░░░░░░░░░░░    │
│   ║ │ TERMINAL  │ ║     │    │  AREA    │     │                                 │
│   ║ └───────────┘ ║     │    └──────────┘     │      ┌──────────────────┐       │
│   ║               ║     │                     │      │    MCP2515       │       │
│   ║ [D2]TVS [D1]  ║     │   [U1]              │      │    CAN MODULE    │       │
│   ║               ║     └─────────────────────┘      │      (J3)        │       │
│   ║  ┌─────────┐  ║               │                  │    7-pin XH      │       │
│   ║  │  BUCK   │  ║         ┌─────┴─────┐            └──────────────────┘       │
│   ║  │  U2     │  ║         │  USB-C    │                    │                  │
│   ║  │ MP2359  │  ║         │   (J2)    │             ┌──────┴──────┐           │
│   ║  └─────────┘  ║         └───────────┘             │  CAN BUS    │ ←─ EDGE   │
│   ║   [L1][C1,2]  ║                                   │    (J4)     │           │
│   ╚═══════════════╝                                   │  4-pin GH   │           │
│        3V3 ───────────────────────────────────────────┤   [JP1]     │           │
│                                                       │  120Ω Term  │           │
├──────────────────────────────────────────────────────┴──────────────┴───────────┤
│   LEFT EDGE                    CENTER ZONE                        RIGHT EDGE    │
│                                                                                  │
│   ╔═════════════════╗     ┌───────────────────────┐      ┌────────────────┐     │
│   ║  ANALOG ZONE    ║     │                       │      │   MICRO SD     │     │
│   ║  (Isolated)     ║     │    MAIN LOGIC AREA    │      │     (J7)       │ ←─  │
│   ║                 ║     │                       │      │   Push-Push    │EDGE │
│   ║  ┌───────────┐  ║     │  [C3-C6] Decoupling   │      │    Slot        │     │
│   ║  │  LM358    │  ║     │                       │      └────────────────┘     │
│   ║  │   U3      │  ║     │  ┌───────────────┐    │                             │
│   ║  │  OP-AMP   │  ║     │  │   MCP23017    │    │      ┌────────────────┐     │
│   ║  └───────────┘  ║     │  │     (U7)      │    │      │     JTAG       │     │
│   ║  [R2,R3] RC     ║     │  │  I2C GPIO     │    │      │    (J9)        │ ←─  │
│   ║                 ║     │  │  EXPANDER     │    │      │  ARM 2×5       │EDGE │
│   ║  ┌───────────┐  ║     │  └───────────────┘    │      │  1.27mm        │     │
│   ║  │  PWM OUT  │  ║     │                       │      └────────────────┘     │
│   ║  │   (J5)    │◄─╫─────┤  [R24,R25] I2C PU     │                             │
│   ║  │  3-pin PH │  ║     │                       │                             │
│   ║  └───────────┘  ║     └───────────────────────┘                             │
│   ╚═════════════════╝                                                           │
│                                                                                  │
├─────────────────────────────────────────────────────────────────────────────────┤
│                              INPUT ISOLATION ZONE                               │
│                                                                                  │
│   ┌─────────────────────┐  ┌─────────────────────┐  ┌─────────────────────┐     │
│   │   RPM INPUT SECTION │  │   VSS INPUT SECTION │  │  WATER TEMP SECTION │     │
│   │                     │  │                     │  │                     │     │
│   │  ┌───────┐ [D3]     │  │  ┌───────┐ [D4]     │  │  [D5]  [R27,28,29]  │     │
│   │  │ PC817 │ [R20,21] │  │  │LM1815 │ [R22,23] │  │  [C17,C18]          │     │
│   │  │  U5   │          │  │  │  U6   │ [C14,15] │  │  Voltage Divider    │     │
│   │  │ OPTO  │          │  │  │ VR IF │          │  │  + RC Filter        │     │
│   │  └───────┘          │  │  └───────┘          │  │                     │     │
│   │  ┌───────┐          │  │  ┌───────┐          │  │  ┌───────┐          │     │
│   │  │  J11  │ ←── EDGE │  │  │  J12  │ ←── EDGE │  │  │  J18  │ ←── EDGE │     │
│   │  │RPM IN │          │  │  │VSS IN │          │  │  │WT IN  │          │     │
│   │  │2-pin  │          │  │  │2-pin  │          │  │  │2-pin  │          │     │
│   │  └───────┘          │  │  └───────┘          │  │  └───────┘          │     │
│   └─────────────────────┘  └─────────────────────┘  └─────────────────────┘     │
│                                                                                  │
│                                                       [RST]    [BOOT]           │
│                                                        SW1      SW2             │
│                                                                                  │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                BOTTOM EDGE                                      │
│                                                                                  │
│   ┌─────────┐  ┌───────────────────────────────────────────────┐  ┌───────────┐ │
│   │  SPI    │  │              ENCODER CONNECTORS               │  │   UART    │ │
│   │ SLAVE   │  │                  (via MCP23017)               │  │  DEBUG    │ │
│   │  (J6)   │  │ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐      │  │  (J10)    │ │
│   │ 6-pin   │  │ │ J13 │ │ J14 │ │ J15 │ │ J16 │ │ J17 │      │  │  6-pin    │ │
│   │   GH    │  │ │ENC1 │ │ENC2 │ │ENC3 │ │ENC4 │ │ENC5 │      │  │  2.54mm   │ │
│   │         │  │ │5-pin│ │5-pin│ │5-pin│ │5-pin│ │5-pin│      │  │          │ │
│   │         │  │ │ XH  │ │ XH  │ │ XH  │ │ XH  │ │ XH  │      │  │          │ │
│   └────┬────┘  │ └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘      │  └────┬─────┘ │
│        ↓       └────┼──────┼───────┼───────┼───────┼──────────┘       ↓       │
│   To Slave          ↓      ↓       ↓       ↓       ↓            Debug Serial  │
│                    Power Steering / Future Expansion                          │
│                                                                                  │
│   ○ ○ ○ ○ ○ ○ ○                                                              ○  │
│   ○ ○ ○ ○ ○ ○ ○   EXPANSION HEADER (J8) - 2×7 Pin                               │
│       TP1-TP7 Test Points nearby                                                │
│  ○                                                                           ○  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### Component Zone Summary

| Zone | Components | Rationale |
|------|------------|-----------|
| **Power Zone** (Top-Left) | J1, U2, D1, D2, L1, C1-C2 | Isolated, thermal relief, short high-current paths |
| **ESP32 Zone** (Top-Center) | U1, USB-C (J2), C3-C6 | Antenna at edge, USB traces <15mm |
| **CAN Zone** (Top-Right) | J3, J4, JP1 | Vehicle interface grouped, easy termination access |
| **Analog Zone** (Mid-Left) | U3, J5, RC filter | Isolated from digital noise, dedicated ground pour |
| **Logic Zone** (Center) | U7, decoupling | Central routing hub for I2C/SPI |
| **Storage Zone** (Mid-Right) | J7, J9 | Edge access for SD card and debug probe |
| **Input Isolation** (Lower) | U5, U6, J11, J12, J18, D3-D5 | Automotive signals isolated from logic |
| **Interface Zone** (Bottom) | J6, J13-J17, J8, J10 | All cable connectors accessible from edge |

### Signal Routing Priority

Route in this order to minimize interference:

1. **USB D+/D-** - Differential pair, 90Ω impedance, length matched ±0.5mm
2. **Power traces** - 3V3 and 12V rails, wide traces (>0.5mm)
3. **SPI buses** - Keep clock lines short, ground guards between buses
4. **I2C bus** - To MCP23017, 10K pull-ups near ESP32
5. **Analog signals** - PWM and filtered output, isolated ground return
6. **RPM/VSS inputs** - From isolation circuits to GPIO, keep short

### Thermal Management

```
┌─────────────────────────────────────┐
│  THERMAL RELIEF AREA                │
│                                     │
│  ┌─────────┐                        │
│  │  BUCK   │ ← Place thermal vias   │
│  │   U2    │   under inductor pad   │
│  └─────────┘                        │
│                                     │
│  ▓▓▓▓▓▓▓▓▓▓▓ ← Copper pour for      │
│  ▓▓▓▓▓▓▓▓▓▓▓   heat spreading       │
│  ▓▓▓▓▓▓▓▓▓▓▓   (both layers)        │
│                                     │
│  Keep 5mm clearance to ESP32 module │
└─────────────────────────────────────┘
```

- Buck converter (U2): Add 6-9 thermal vias (0.3mm) under exposed pad
- Ground pour on bottom layer acts as heat sink
- Maintain 5mm gap between power section and ESP32 module

### Connector Placement Summary

| Connector | Edge | Position | Cable Direction |
|-----------|------|----------|-----------------|
| J1 (12V Power) | Top-Left | Vertical | Up/Left |
| J2 (USB-C) | Top | Horizontal | Up |
| J3 (MCP2515 Module) | Top-Right | Vertical | Up |
| J4 (CAN Bus) | Right | Horizontal | Right |
| J5 (PWM Output) | Left | Horizontal | Left |
| J6 (SPI Slave) | Bottom-Left | Vertical | Down |
| J7 (Micro SD) | Right | Push-push | Right |
| J8 (Expansion) | Bottom | Vertical | Down |
| J9 (JTAG) | Right | Horizontal | Right |
| J10 (UART Debug) | Bottom-Right | Horizontal | Down |
| J11 (RPM Input) | Bottom-Left | Vertical | Down |
| J12 (VSS Input) | Bottom-Center | Vertical | Down |
| J13-J17 (Encoders) | Bottom | Vertical | Down |
| J18 (Water Temp) | Bottom-Center | Vertical | Down |

### Test Point Locations

Place test points in accessible locations near board edge:

```
TP1 (3V3)  ─── Near power section output
TP2 (12V)  ─── Near J1 input
TP3 (RC)   ─── Between RC filter and op-amp input
TP4 (PWM)  ─── Op-amp output, before J5
TP5 (INT)  ─── MCP2515 interrupt line
TP6 (EN)   ─── ESP32 enable/reset
TP7 (GND)  ─── Reference ground near expansion header
```
```

## Firmware Compatibility

This custom PCB is 100% pin-compatible with the existing firmware for core functions. The SD card and RPM input are integrated features with dedicated firmware modules.

### Native USB (GPIO 19/20)

Enable in `platformio.ini`:

```ini
build_flags =
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
```

### SD Card Pin Definitions

Add to `config.h`:

```cpp
// Master - SD Card (SPI Mode)
#define SD_CS_PIN   15
#define SD_MOSI_PIN 16
#define SD_SCK_PIN  17
#define SD_MISO_PIN 18
#define SD_CD_PIN   8    // Optional card detect (LOW = card present)
#define SD_SPI_FREQ 25000000  // 25 MHz
```

### SD Card Initialization Example

```cpp
#include <SD.h>
#include <SPI.h>

SPIClass sdSPI(HSPI);

bool initSDCard() {
    sdSPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

    if (!SD.begin(SD_CS_PIN, sdSPI, SD_SPI_FREQ)) {
        Serial.println("SD Card initialization failed");
        return false;
    }

    Serial.printf("SD Card: %lluMB\n", SD.cardSize() / (1024 * 1024));
    return true;
}
```

### RPM Input Pin Definition

Already defined in `config.h`:

```cpp
// Master - RPM Input (12V square wave via level shifter)
// Expects 1 pulse per revolution, level-shifted from 12V to 3.3V
// Recommended circuit: Optocoupler (PC817) or transistor level shifter
#define RPM_INPUT_PIN      1    // GPIO 1 - available input pin
```

### RPM Counter Usage Example

```cpp
#include "rpm_counter.h"

void setup() {
    Serial.begin(115200);

    // Initialize RPM counter (starts disabled)
    rpmCounterInit();

    // Enable when ready to read RPM
    rpmCounterEnable();
}

void loop() {
    // Get current RPM reading
    float rpm = rpmCounterGetRPM();

    if (rpm > 0) {
        Serial.printf("Engine RPM: %.0f\n", rpm);
    }

    delay(100);  // 10Hz update rate
}
```

### RPM Counter Serial Commands

| Command | Action |
|---------|--------|
| `p` | Enable counter / Show current RPM reading |
| `P` | Disable counter (releases PCNT resources) |

## Cable Wiring Reference

### J4 - CAN Bus Cable (JST-GH 4-pin)

```
Board Side          Cable          Vehicle Side
─────────────────────────────────────────────────
Pin 1 (CANH) ────── Orange ──────── CAN High
Pin 2 (CANL) ────── Yellow ──────── CAN Low
Pin 3 (12V)  ────── Red ─────────── +12V (OBD pin 16)
Pin 4 (GND)  ────── Black ────────── Ground (OBD pin 4/5)
```

### J5 - PWM Output Cable (JST-PH 3-pin)

```
Board Side          Cable          Motor Controller
─────────────────────────────────────────────────
Pin 1 (SIG)  ────── Yellow ──────── Analog In / Wiper
Pin 2 (5V)   ────── Red ─────────── 5V Ref (if needed)
Pin 3 (GND)  ────── Black ────────── Ground
```

### J6 - SPI Slave Cable (JST-GH 6-pin)

```
Board Side          Cable          Slave Board
─────────────────────────────────────────────────
Pin 1 (MOSI) ────── Orange ──────── MOSI (GPIO 2)
Pin 2 (MISO) ────── Yellow ──────── MISO (GPIO 3)
Pin 3 (SCK)  ────── Green ────────── SCK (GPIO 14)
Pin 4 (CS)   ────── Blue ─────────── CS (GPIO 21)
Pin 5 (3V3)  ────── Red ──────────── 3V3
Pin 6 (GND)  ────── Black ────────── GND
```

### J11 - RPM Input Cable (JST-PH 2-pin)

```
Board Side          Cable          Signal Source
─────────────────────────────────────────────────
Pin 1 (SIG)  ────── White ────────── Tach signal (12V square wave)
Pin 2 (GND)  ────── Black ────────── Signal ground

Signal Sources:
- ECU tachometer output (cleanest signal)
- Ignition coil negative terminal
- Aftermarket tachometer adapter
- Crank position sensor output

Wire Recommendations:
- Use twisted pair cable for noise rejection
- Keep cable under 2 meters if possible
- Use shielded cable near ignition components
- Connect shield to GND at board end only
```

### J12 - VSS Input Cable (JST-PH 2-pin)

```
Board Side          Cable          Sensor Side
─────────────────────────────────────────────────
Pin 1 (VR+)  ────── White ────────── VSS Signal (AC)
Pin 2 (VR-)  ────── Black ────────── VSS Ground (transmission case)

Sensor Location:
- GM 700R4 transmission tail housing
- 2-wire Variable Reluctance (VR) sensor
- 8000 pulses per mile

Wire Recommendations:
- 20-22 AWG twisted pair (shielded preferred)
- Maximum length: 5 meters
- Shield connects to GND at board end only
- Keep away from ignition wires
- Leave service loop near transmission
```

### J13-J17 - Encoder Cables (JST-XH 5-pin)

All five encoder connectors (J13-J17) share the same pinout.
Encoders connect via MCP23017 I2C GPIO expander.

```
Board Side          Cable          Encoder Side
─────────────────────────────────────────────────
Pin 1 (CLK)  ────── White ────────── A / CLK
Pin 2 (DT)   ────── Green ────────── B / DT
Pin 3 (SW)   ────── Blue ─────────── Switch (active LOW)
Pin 4 (3V3)  ────── Red ──────────── VCC / +
Pin 5 (GND)  ────── Black ────────── GND / -

Connector Assignments:
┌──────┬────────────────────────────────────────────────┐
│ J13  │ Power Steering Speed - Main control encoder    │
│ J14  │ Future Use - Available for expansion           │
│ J15  │ Future Use - Available for expansion           │
│ J16  │ Future Use - Available for expansion           │
│ J17  │ Future Use - Available for expansion           │
└──────┴────────────────────────────────────────────────┘

Compatible Encoders:
- KY-040 rotary encoder module
- EC11 panel-mount encoders
- Alps EC11 series
- Bourns PEC11 series

Wire Recommendations:
- 24-28 AWG stranded wire
- Keep cables under 50cm for reliable operation
- Shielded cable recommended for panel-mount encoders
- Use connector on both ends for easy replacement
```
