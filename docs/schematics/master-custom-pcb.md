# Master Custom PCB Schematic

Custom PCB design for the ESP32-S3 master board with integrated USB programming, power regulation, and all peripheral interfaces.

## Design Overview

This schematic replaces the ESP32-S3-DevKitC-1 with a custom PCB featuring:
- ESP32-S3-WROOM-1-N16R8 module (16MB Flash, 8MB PSRAM)
- Single USB-C connector using ESP32-S3 native USB
- 12V automotive input with efficient power regulation
- Integrated CAN bus transceiver (replaces MCP2515 module)
- On-board PWM-to-analog conversion circuit
- Rotary encoder interface
- SPI connector for slave communication
- Micro SD card slot for data logging

## Connector Summary

All external peripheral connections use locking cable connectors for reliable automotive use:

| Connector | Type | Pins | Function |
|-----------|------|------|----------|
| J1 | Screw Terminal (5.08mm) | 2 | 12V Power Input |
| J2 | USB-C Receptacle | - | Programming/Debug (Native USB) |
| J3 | JST-XH 7-pin | 7 | MCP2515 CAN Module |
| J4 | JST-GH 4-pin | 4 | CAN Bus (CANH/CANL/12V/GND) |
| J5 | JST-XH 5-pin | 5 | Rotary Encoder |
| J6 | JST-PH 3-pin | 3 | PWM Output (SIG/5V/GND) |
| J7 | JST-GH 6-pin | 6 | SPI Slave Communication |
| J8 | Micro SD Slot | - | SD Card Storage |
| J9 | Pin Header 2×7 | 14 | Expansion GPIOs |
| J10 | ARM Cortex 2×5 (1.27mm) | 10 | JTAG Debug Interface |
| J11 | Pin Header 1×6 (2.54mm) | 6 | UART Debug + Boot Control |

## System Block Diagram

```
                    ┌──────────────────────────────────────────────────────────────────┐
     12V IN         │                    MASTER CUSTOM PCB                             │
        │           │                                                                  │
        ▼           │   ┌─────────┐    ┌─────────────────────────────────────┐        │
   ┌─────────┐      │   │  USB-C  │    │     ESP32-S3-WROOM-1-N16R8         │        │
   │ MP2359  │──3.3V──►│  (J2)   │◄──►│                                     │        │
   │  Buck   │      │   │         │    │  GPIO 19/20: USB D+/D-             │        │
   └────┬────┘      │   └─────────┘    │  SPI1: MCP2515 (CAN)               │        │
        │           │                   │  SPI2: Slave Communication          │        │
       12V──────────│───────────────────│  SPI3: SD Card                      │        │
        │           │   ┌─────────┐    │  GPIO 4-6: Rotary Encoder           │        │
        ▼           │   │ MCP2515 │◄──►│  GPIO 7: PWM Output                 │        │
   ┌─────────┐      │   │   CAN   │    └─────────────────────────────────────┘        │
   │  LM358  │◄─────│───│ + TJA   │                    │                              │
   │ Op-Amp  │      │   │  (J3)   │            ┌───────┴───────┐                      │
   └────┬────┘      │   └────┬────┘            │   Micro SD    │                      │
        │           │        │                 │     (J8)      │                      │
        ▼           │        ▼                 └───────────────┘                      │
   ┌─────────┐      │   ┌─────────┐                                                   │
   │ JST-PH  │      │   │ JST-GH  │     ┌─────────┐        ┌─────────┐               │
   │PWM (J6) │      │   │CAN (J4) │     │ JST-XH  │        │ JST-GH  │               │
   └────┬────┘      │   └────┬────┘     │ENC (J5) │        │SPI (J7) │               │
        │           │        │          └────┬────┘        └────┬────┘               │
        ▼           │        ▼               │                  │                    │
   Motor Ctrl       │    CAN Bus         Rotary              Slave                   │
   (0-5V)           │  (CANH/CANL)       Encoder             Board                   │
                    └──────────────────────────────────────────────────────────────────┘
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

### Sheet 5: Rotary Encoder Interface

```
                              ROTARY ENCODER (KY-040) INTERFACE
════════════════════════════════════════════════════════════════════════════════════

    The KY-040 module has built-in pull-up resistors.
    For a custom PCB without the module, add pull-ups.

    ROTARY ENCODER CONNECTOR (J5) - JST-XH 5-Pin Locking Connector
    ══════════════════════════════════════════════════════════════

    JST-XH (2.5mm pitch) - robust, polarized, easy hand assembly

                    JST-XH-5P (J5)
                    ┌───────────────────────────┐
                    │ ┌───┬───┬───┬───┬───┐     │
                    │ │ 1 │ 2 │ 3 │ 4 │ 5 │     │  ← Polarization key
                    │ └─┬─┴─┬─┴─┬─┴─┬─┴─┬─┘     │
                    └───┼───┼───┼───┼───┼───────┘
                        │   │   │   │   │
                       CLK  DT  SW  3V3 GND
                       (A)  (B)

    Pin Assignment:
    ┌─────┬────────┬─────────────────────────────────────┐
    │ Pin │ Signal │ Description                         │
    ├─────┼────────┼─────────────────────────────────────┤
    │  1  │ CLK    │ Encoder Channel A (GPIO 4)          │
    │  2  │ DT     │ Encoder Channel B (GPIO 5)          │
    │  3  │ SW     │ Push button (GPIO 6, active LOW)    │
    │  4  │ 3V3    │ 3.3V power for encoder              │
    │  5  │ GND    │ Ground                              │
    └─────┴────────┴─────────────────────────────────────┘

    Mating Connector: JST XHP-5 (housing) + SXH-001T-P0.6 (crimp terminals)
    Wire: 24-28 AWG, shielded cable recommended for noise immunity

    Compatible with KY-040 module or panel-mount encoders (EC11, etc.)


    Option B: Integrated Encoder Circuit
    ════════════════════════════════════

                                    3V3
                                     │
                    ┌────────────────┼────────────────┐
                    │                │                │
                   ┌┴┐              ┌┴┐              ┌┴┐
               10K │ │          10K │ │          10K │ │
                   └┬┘              └┬┘              └┬┘
                    │                │                │
    GPIO 4 ─────────┼────────────────│────────────────│────────► ENC_CLK
                    │                │                │
                   ═══              ═══              ═══
                  100nF            100nF            100nF  (Debounce)
                    │                │                │
                    │                │                │
                   ─┴─              ─┴─              ─┴─
                  ╱   ╲            ╱   ╲            ╱   ╲
                 ─┬───┬─          ─┬───┬─          ─┬───┬─  Encoder Switches
                    │                │                │
                   GND              GND              GND

    GPIO 5 ─────────────────────────┼─────────────────────────► ENC_DT
                                    │
    GPIO 6 ─────────────────────────────────────────┼─────────► ENC_SW


    ENCODER CONNECTOR (J5) - For panel-mount encoder
    ════════════════════════════════════════════════

                    ┌─────────────────┐
                    │  A   B   C  SW  │
                    │  ○   ○   ○   ○  │
                    └──┬───┬───┬───┬──┘
                       │   │   │   │
                   ENC_A│ ENC_B│ GND │
                       │   │       │
                       │   │    ENC_SW
                       │   │
                  GPIO4┘ GPIO5┘

    Note: Pin C is common (GND) for the encoder
          SW pin connects to ENC_SW through pull-up
```

### Sheet 6: PWM Motor Controller Interface

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
    GPIO 7 ───────────┤►  R1   ├───┬───┤► 3+ OUT 1 ├───┬──────────────────► PWM_OUT (J6)
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


    PWM OUTPUT CONNECTOR (J6) - JST-PH 3-Pin Locking Connector
    ═══════════════════════════════════════════════════════════

    JST-PH (2.0mm pitch) - compact, secure, common in hobby electronics

                    JST-PH-3P (J6)
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

### Sheet 7: Slave Communication (SPI)

```
                              SPI INTERFACE TO SLAVE BOARD
════════════════════════════════════════════════════════════════════════════════════

    Full-duplex SPI at 1MHz for master-slave communication

    SPI CONNECTOR (J7) - JST-GH 6-Pin Locking Connector
    ═══════════════════════════════════════════════════

    JST-GH (1.25mm pitch) - compact, locking, reliable for data cables

                    JST-GH-6P (J7)
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
    GPIO 2  ─────────┤►├────────────────────► J7 Pin 1 (MOSI)

                      33Ω
    GPIO 14 ─────────┤►├────────────────────► J7 Pin 3 (SCK)

                      33Ω
    GPIO 21 ─────────┤►├────────────────────► J7 Pin 4 (CS)

    GPIO 3  ◄──────────────────────────────── J7 Pin 2 (MISO)
                                              (no termination on input)
```

### Sheet 8: Micro SD Card Interface

```
                              MICRO SD CARD INTERFACE (SPI Mode)
════════════════════════════════════════════════════════════════════════════════════

    SD card in SPI mode for data logging and configuration storage.
    Uses dedicated SPI bus on available GPIOs.

    SD CARD PINOUT (J8)
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

### Sheet 9: Expansion & Test Points

```
                              EXPANSION HEADER & TEST POINTS
════════════════════════════════════════════════════════════════════════════════════

    EXPANSION HEADER (J9) - Available GPIOs
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

### Sheet 10: Diagnostic Interfaces (JTAG & UART)

```
                              DIAGNOSTIC INTERFACES
════════════════════════════════════════════════════════════════════════════════════

    JTAG DEBUG HEADER (J10) - ARM Cortex 10-Pin (1.27mm pitch)
    ═══════════════════════════════════════════════════════════

    Standard ARM Cortex debug connector for JTAG/SWD debugging.
    Compatible with ESP-Prog, J-Link, and other JTAG adapters.

    ESP32-S3 JTAG Pins:
    - GPIO 39: MTCK (TCK) - Test Clock
    - GPIO 40: MTDO (TDO) - Test Data Out
    - GPIO 41: MTDI (TDI) - Test Data In
    - GPIO 42: MTMS (TMS) - Test Mode Select

                    ARM Cortex 10-Pin Header (J10)
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


    UART DEBUG HEADER (J11) - 6-Pin (2.54mm pitch)
    ══════════════════════════════════════════════

    Secondary serial port for diagnostics when USB is unavailable.
    Uses UART1 on available GPIOs (independent of USB CDC).

    ESP32-S3 UART Pins:
    - GPIO 43: U0TXD (default UART0 TX)
    - GPIO 44: U0RXD (default UART0 RX)

                    UART Header (J11)
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
| 0 | BOOT | I | Boot button / J11 Pin 6 | Strapping pin (LOW=bootloader) |
| 1 | - | - | Expansion | Available |
| 2 | COMM_SPI_MOSI | O | Slave MOSI (J7) | SPI to slave |
| 3 | COMM_SPI_MISO | I | Slave MISO (J7) | SPI from slave |
| 4 | ENC_CLK | I | Encoder CLK (J5) | Rotary encoder A |
| 5 | ENC_DT | I | Encoder DT (J5) | Rotary encoder B |
| 6 | ENC_SW | I | Encoder SW (J5) | Encoder button |
| 7 | PWM_OUT | O | RC Filter (R1) | PWM motor control |
| 8 | SD_CD | I | SD Card Detect (J8) | Optional card detect |
| 9 | MCP_INT | I | MCP2515 INT (J3) | CAN interrupt |
| 10 | MCP_CS | O | MCP2515 CS (J3) | CAN chip select |
| 11 | MCP_MOSI | O | MCP2515 SI (J3) | CAN SPI data out |
| 12 | MCP_SCK | O | MCP2515 SCK (J3) | CAN SPI clock |
| 13 | MCP_MISO | I | MCP2515 SO (J3) | CAN SPI data in |
| 14 | COMM_SPI_SCK | O | Slave SCK (J7) | SPI clock to slave |
| 15 | SD_CS | O | SD Card CS (J8) | SD card chip select |
| 16 | SD_MOSI | O | SD Card MOSI (J8) | SD card data out |
| 17 | SD_SCK | O | SD Card SCK (J8) | SD card clock |
| 18 | SD_MISO | I | SD Card MISO (J8) | SD card data in |
| 19 | USB_D- | I/O | USB-C D- | Native USB |
| 20 | USB_D+ | I/O | USB-C D+ | Native USB |
| 21 | COMM_SPI_CS | O | Slave CS (J7) | SPI chip select |
| 22-25 | - | - | Expansion | Available |
| 26-32 | FLASH | - | Internal | Reserved - DO NOT USE |
| 33-37 | PSRAM | - | Internal | Reserved - DO NOT USE |
| 38 | - | - | Expansion | Available |
| 39 | JTAG_TCK | I/O | J10 Pin 4 | JTAG Test Clock |
| 40 | JTAG_TDO | O | J10 Pin 6 | JTAG Test Data Out |
| 41 | JTAG_TDI | I | J10 Pin 8 | JTAG Test Data In |
| 42 | JTAG_TMS | I/O | J10 Pin 2 | JTAG Test Mode Select |
| 43 | UART_TX | O | J11 Pin 3 | Debug UART transmit |
| 44 | UART_RX | I | J11 Pin 4 | Debug UART receive |
| 45-48 | - | - | Expansion | Available |
| EN | Reset | I | Reset btn / J10 Pin 10 / J11 Pin 5 | Active low reset |

## Bill of Materials (BOM)

### Integrated Circuits

| Ref | Part | Package | Qty | Notes |
|-----|------|---------|-----|-------|
| U1 | ESP32-S3-WROOM-1-N16R8 | Module | 1 | 16MB Flash, 8MB PSRAM |
| U2 | MP2359DJ-LF-Z | SOT23-6 | 1 | Buck regulator (or MP1584 module) |
| U3 | LM358P | DIP-8/SOIC-8 | 1 | Dual op-amp |
| U4 | USBLC6-2SC6 | SOT23-6 | 1 | USB ESD protection |

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
| R5-R7 | 10K | 0402/0603 | 3 | Pull-ups (EN, IO0, encoder) |
| R8-R9 | 47K | 0402/0603 | 2 | SD MISO, Card detect pull-up |

### Connectors (Locking Cable Connectors)

| Ref | Type | Part Number | Pins | Notes |
|-----|------|-------------|------|-------|
| J1 | Screw Terminal 5.08mm | Phoenix 1935161 | 2 | 12V power input (up to 10A) |
| J2 | USB-C Receptacle | GCT USB4110 | 16 | USB programming/debug |
| J3 | JST-XH Vertical | B7B-XH-A | 7 | MCP2515 CAN module |
| J4 | JST-GH Vertical | SM04B-GHS-TB | 4 | CAN bus cable (CANH/CANL/12V/GND) |
| J5 | JST-XH Vertical | B5B-XH-A | 5 | Rotary encoder cable |
| J6 | JST-PH Vertical | B3B-PH-K-S | 3 | PWM output (SIG/5V/GND) |
| J7 | JST-GH Vertical | SM06B-GHS-TB | 6 | SPI slave cable |
| J8 | Micro SD Slot | Molex 504077 | 8+1 | Push-push micro SD |
| J9 | Pin Header 2.54mm | - | 2×7 | Expansion GPIOs |
| J10 | ARM Cortex Debug 1.27mm | Samtec FTSH-105-01 | 2×5 | JTAG/SWD debug (shrouded) |
| J11 | Pin Header 2.54mm RA | - | 1×6 | UART debug (right-angle) |

### Mating Cable Connectors (for harness assembly)

| For | Housing | Crimp Terminal | Notes |
|-----|---------|----------------|-------|
| J3 (MCP2515) | XHP-7 | SXH-001T-P0.6 | JST-XH 7-pos |
| J4 (CAN bus) | GHR-04V-S | SSH-003T-P0.2 | JST-GH 4-pos |
| J5 (Encoder) | XHP-5 | SXH-001T-P0.6 | JST-XH 5-pos |
| J6 (PWM out) | PHR-3 | SPH-002T-P0.5S | JST-PH 3-pos |
| J7 (SPI slave) | GHR-06V-S | SSH-003T-P0.2 | JST-GH 6-pos |

### Miscellaneous

| Ref | Part | Qty | Notes |
|-----|------|-----|-------|
| SW1 | Tactile Switch 6mm | 1 | Reset button |
| SW2 | Tactile Switch 6mm | 1 | Boot button (optional) |
| D1 | SS34 | 1 | Reverse polarity protection |
| D2 | SMBJ15A | 1 | TVS surge protection |
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

### Board Dimensions

Suggested size: 75mm × 55mm (accommodates SD card, debug headers, and all connectors)

```
┌────────────────────────────────────────────────────────────────────────────┐
│  ○                                                                      ○  │
│     ┌──────┐                              ┌──────────────┐                  │
│     │USB-C │    ESP32-S3-WROOM-1          │  MCP2515     │  ┌─────┐        │
│     │ (J2) │    ┌──────────────┐          │  Module (J3) │  │ CAN │        │
│     └──────┘    │              │          └──────────────┘  │(J4) │        │
│                 │   Antenna    │                            │JST  │        │
│  ┌─────────┐    │    Keep      │          ┌────────────┐    └─────┘        │
│  │12V Power│    │    Clear     │          │ Micro SD   │                   │
│  │  (J1)   │    │              │          │   (J8)     │    ┌─────────┐    │
│  │ Screw   │    └──────────────┘          └────────────┘    │ JTAG    │    │
│  └─────────┘                                                │ (J10)   │    │
│                                                             │ 2×5     │    │
│  ┌─────┐   ┌─────┐        [RST] [BOOT]                      └─────────┘    │
│  │ PWM │   │ SPI │                                                         │
│  │(J6) │   │(J7) │   ○ ○ ○ ○ ○ ○ ○        ┌─────────┐   ○ ○ ○ ○ ○ ○        │
│  │JST  │   │JST  │   ○ ○ ○ ○ ○ ○ ○        │Encoder  │   UART (J11)         │
│  └─────┘   └─────┘   Expansion (J9)       │  (J5)   │   1×6 horiz          │
│                                           │ JST-XH  │                      │
│  ○                                        └─────────┘                   ○  │
└────────────────────────────────────────────────────────────────────────────┘

Mounting holes: 4× M3 at corners (3.2mm diameter)

Connector Placement Notes:
- USB-C (J2): Top edge, centered for easy access during development
- 12V Power (J1): Left edge, screw terminal for secure connection
- CAN Bus (J4): Right edge, near MCP2515 module
- SD Card (J8): Right side, push-push slot, accessible from edge
- PWM/SPI (J6, J7): Bottom left, grouped for cable management
- Encoder (J5): Bottom right, panel-mount encoder cable
- Expansion (J9): Bottom center, 2×7 pin header
- JTAG (J10): Right side, accessible for debug probe connection
- UART (J11): Bottom right, horizontal for easy cable routing
```

## Firmware Compatibility

This custom PCB is 100% pin-compatible with the existing firmware for core functions. The SD card is a new addition requiring config.h updates.

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

### J5 - Encoder Cable (JST-XH 5-pin)

```
Board Side          Cable          Encoder Side
─────────────────────────────────────────────────
Pin 1 (CLK)  ────── White ────────── A / CLK
Pin 2 (DT)   ────── Green ────────── B / DT
Pin 3 (SW)   ────── Blue ─────────── Switch
Pin 4 (3V3)  ────── Red ──────────── VCC / +
Pin 5 (GND)  ────── Black ────────── GND / -
```

### J6 - PWM Output Cable (JST-PH 3-pin)

```
Board Side          Cable          Motor Controller
─────────────────────────────────────────────────
Pin 1 (SIG)  ────── Yellow ──────── Analog In / Wiper
Pin 2 (5V)   ────── Red ─────────── 5V Ref (if needed)
Pin 3 (GND)  ────── Black ────────── Ground
```

### J7 - SPI Slave Cable (JST-GH 6-pin)

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
