# Master ESP32-S3 Complete Schematic

Complete wiring diagram for the master ESP32-S3-N16R8 including power supply, CAN bus, rotary encoder, SPI communication to slave, and PWM motor controller interface.

## ESP32-S3-N16R8 Specifications

| Feature | Value |
|---------|-------|
| Module | ESP32-S3-N16R8 |
| Flash | 16MB (Quad SPI) |
| PSRAM | 8MB (Octal SPI) |
| CPU | Dual-core Xtensa LX7 @ 240MHz |
| SRAM | 512KB |

**Reserved Pins (Do Not Use):**
- GPIO 26-32: Internal flash interface
- GPIO 33-37: Octal PSRAM interface

## System Block Diagram

```
                                    ┌─────────────────────────────────────────┐
                                    │       MASTER ESP32-S3-N16R8             │
                                    │                                         │
    ┌───────────┐                   │  ┌─────────┐         ┌─────────┐       │
    │  12V      │                   │  │  SPI1   │         │  SPI2   │       │
    │  Supply   ├───┬───────────────┼─►│(FSPI)   │         │ (HSPI)  │       │
    └───────────┘   │               │  │         │         │         │       │
                    │               │  │ MCP2515 │         │ Slave   │       │
              ┌─────┴─────┐         │  │ CAN Bus │         │ Comm    │       │
              │ AMS1117   │         │  └─────────┘         └─────────┘       │
              │ 3.3V Reg  │         │                                         │
              └─────┬─────┘         │  ┌─────────┐         ┌─────────┐       │
                    │               │  │ Encoder │         │   PWM   │       │
                   3.3V ────────────┼─►│ KY-040  │         │ Output  │       │
                                    │  └─────────┘         └─────────┘       │
                                    └─────────────────────────────────────────┘
                                              │                   │
                                              ▼                   ▼
                                    ┌─────────────────┐   ┌───────────────┐
                                    │  Vehicle CAN    │   │  PWM Motor    │
                                    │  Bus            │   │  Controller   │
                                    └─────────────────┘   └───────────────┘
```

## Complete Circuit Schematic

```
12V SUPPLY
    │
    │    POWER SUPPLY SECTION
    │    ════════════════════
    │
    ├────────────────────────────────────────────────────────────────► 12V (to op-amp)
    │
    │         AMS1117-3.3
    │        ┌───────────┐
    ├────┬───┤ IN    OUT ├───┬──────────────────────────────────────► 3.3V
    │    │   │           │   │
    │   ═══  │    GND    │  ═══  100nF
    │  10µF  │     │     │  10µF  ═══
    │    │   └─────┼─────┘   │     │
    │    │         │         │     │
────┴────┴─────────┴─────────┴─────┴────────────────────────────────► GND



                          ESP32-S3-DevKitC-1
    ┌──────────────────────────────────────────────────────────────────┐
    │                                                                  │
    │                              3V3  GND                            │
    │                               │    │                             │
    │  ┌────────────────────────────┼────┼─────────────────────────┐   │
    │  │                            │    │                         │   │
    │  │    ┌───┐ ┌───┐ ┌───┐ ┌───┐│┌───┐│┌───┐ ┌───┐ ┌───┐ ┌───┐ │   │
    │  │    │EN │ │ 4 │ │ 5 │ │ 6 │││3V3│││GND│ │ 7 │ │ 8 │ │ 9 │ │   │
    │  │    └─┬─┘ └─┬─┘ └─┬─┘ └─┬─┘│└───┘│└───┘ └─┬─┘ └───┘ └─┬─┘ │   │
    │  │      │     │     │     │  │     │        │           │   │   │
    │  │      │   ENC_A ENC_B ENC_SW     │      PWM_OUT    MCP_INT │   │
    │  │      │     │     │     │        │        │           │   │   │
    │  │    ┌─┴─┐ ┌─┴─┐ ┌─┴─┐ ┌─┴─┐    ┌─┴─┐    ┌─┴─┐       ┌─┴─┐ │   │
    │  │    │10 │ │11 │ │12 │ │13 │    │14 │    │21 │       │ 2 │ │   │
    │  │    └─┬─┘ └─┬─┘ └─┬─┘ └─┬─┘    └─┬─┘    └─┬─┘       └─┬─┘ │   │
    │  │      │     │     │     │        │        │           │   │   │
    │  │   MCP_CS MCP_MO MCP_CK MCP_MI COMM_CK COMM_CS     COMM_MO│   │
    │  │      │     │     │     │        │        │           │   │   │
    │  │    ┌─┴─┐                      ┌─┴─┐    ┌─┴─┐       ┌─┴─┐ │   │
    │  │    │ 3 │                      │   │    │   │       │   │ │   │
    │  │    └─┬─┘                      └───┘    └───┘       └───┘ │   │
    │  │      │                                                   │   │
    │  │   COMM_MI                                                │   │
    │  │      │                                                   │   │
    │  └──────┼───────────────────────────────────────────────────┘   │
    │         │                                                       │
    └─────────┼───────────────────────────────────────────────────────┘
              │



    MCP2515 CAN CONTROLLER                      ROTARY ENCODER KY-040
    ══════════════════════                      ══════════════════════

           MCP2515 Module                              KY-040
          ┌─────────────┐                         ┌───────────┐
    3.3V ─┤ VCC     INT ├─── GPIO 9               │    ┌─┐    │
          │             │                    3.3V ─┤VCC │●│ SW ├─── GPIO 6
  GPIO 12 ─┤ SCK     SO ├─── GPIO 13              │    └┬┘    │
          │             │                         │     │     │
  GPIO 11 ─┤ SI      CS ├─── GPIO 10         GND ─┤GND  │  DT ├─── GPIO 5
          │             │                         │     │     │
     GND ─┤ GND         │                         │    CLK    ├─── GPIO 4
          │             │                         └─────┬─────┘
          │   CANH CANL │                               │
          └─────┬───┬───┘                              GND
                │   │
                │   │ (to vehicle CAN bus)
               ─┴─ ─┴─



    SPI TO SLAVE                                PWM MOTOR CONTROLLER INTERFACE
    ════════════                                ══════════════════════════════

    To Slave ESP32-S3:                                          12V
                                                                 │
    GPIO 2  (MOSI) ────────► Slave GPIO 2                        │  LM358P
    GPIO 3  (MISO) ◄──────── Slave GPIO 3                        │ ┌──────────┐
    GPIO 14 (SCK)  ────────► Slave GPIO 14          RC Filter    └►│8 VCC     │
    GPIO 21 (CS)   ────────► Slave GPIO 21         ┌────────┐      │          │
                                           GPIO 7 ─┤10K  1µF├─────►│3+    OUT1├──► Wiper
                                             (PWM) └─┬────┬─┘  ┌───┤2-       1│    (0-5V)
                                                    GND  GND   │   │          │
                                                              5.1K │4 GND     │
                                                               │   └────┬─────┘
                                                               ├────────┘
                                                               │
                                                              10K
                                                               │
                                                              GND
```

## Pin Assignment Summary

### ESP32-S3-N16R8 Master Pin Map

| GPIO | Function | Connected To | Notes |
|------|----------|--------------|-------|
| 2 | COMM_SPI_MOSI | Slave GPIO 2 | SPI data to slave |
| 3 | COMM_SPI_MISO | Slave GPIO 3 | SPI data from slave |
| 4 | ENCODER_CLK | KY-040 CLK | Encoder channel A |
| 5 | ENCODER_DT | KY-040 DT | Encoder channel B |
| 6 | ENCODER_SW | KY-040 SW | Encoder button (active low) |
| 7 | PWM_OUTPUT | RC Filter | Motor speed control |
| 9 | MCP2515_INT | MCP2515 INT | CAN interrupt (active low) |
| 10 | MCP2515_CS | MCP2515 CS | CAN chip select |
| 11 | MCP2515_MOSI | MCP2515 SI | CAN SPI data out |
| 12 | MCP2515_SCK | MCP2515 SCK | CAN SPI clock |
| 13 | MCP2515_MISO | MCP2515 SO | CAN SPI data in |
| 14 | COMM_SPI_SCK | Slave GPIO 14 | SPI clock to slave |
| 21 | COMM_SPI_CS | Slave GPIO 21 | SPI chip select to slave |
| 3V3 | Power | All 3.3V devices | From AMS1117 |
| GND | Ground | All grounds | Common ground |

### Power Rails

| Rail | Source | Consumers |
|------|--------|-----------|
| 12V | External supply | AMS1117 input, LM358 VCC |
| 3.3V | AMS1117 output | ESP32-S3, MCP2515, KY-040 |
| 5V | - | Not used on master (PWM ctrl has own supply) |

### GPIO Availability (ESP32-S3-N16R8)

| GPIO | Status | Usage |
|------|--------|-------|
| 0 | Available | (Boot strapping - use with care) |
| 1 | Available | USB D- if USB enabled |
| 2 | **Used** | COMM_SPI_MOSI |
| 3 | **Used** | COMM_SPI_MISO |
| 4 | **Used** | ENCODER_CLK |
| 5 | **Used** | ENCODER_DT |
| 6 | **Used** | ENCODER_SW |
| 7 | **Used** | PWM_OUTPUT |
| 8 | Available | - |
| 9 | **Used** | MCP2515_INT |
| 10 | **Used** | MCP2515_CS |
| 11 | **Used** | MCP2515_MOSI |
| 12 | **Used** | MCP2515_SCK |
| 13 | **Used** | MCP2515_MISO |
| 14 | **Used** | COMM_SPI_SCK |
| 15-25 | Available | Free for expansion |
| 26-32 | **Reserved** | Internal flash (do not use) |
| 33-37 | **Reserved** | Octal PSRAM (do not use) |
| 38-48 | Available | Free for expansion |

**Available for future use:** GPIO 0, 1, 8, 15-25, 38-48

## Complete Parts List

### Power Supply

| Component | Value | Package | Notes |
|-----------|-------|---------|-------|
| U1 | AMS1117-3.3 | SOT-223 | 3.3V LDO regulator |
| C1 | 10µF | 0805/radial | Input capacitor |
| C2 | 10µF | 0805/radial | Output capacitor |
| C3 | 100nF | 0805 | Decoupling, near ESP32 |

### CAN Bus Interface

| Component | Value | Package | Notes |
|-----------|-------|---------|-------|
| U2 | MCP2515 module | - | Includes crystal & transceiver |

### Rotary Encoder

| Component | Value | Package | Notes |
|-----------|-------|---------|-------|
| ENC1 | KY-040 | Module | Includes pull-ups |

### PWM Output Interface

| Component | Value | Package | Notes |
|-----------|-------|---------|-------|
| U3 | LM358P | DIP-8 | Dual op-amp |
| R1 | 10K | 0805/axial | RC filter resistor |
| R2 | 10K | 0805/axial | Gain resistor to GND |
| R3 | 5.1K | 0805/axial | Feedback resistor |
| C4 | 1µF | 0805/radial | RC filter capacitor |

### Connectors

| Connector | Pins | Notes |
|-----------|------|-------|
| J1 | 2 | 12V power input |
| J2 | 2 | CAN bus (CANH, CANL) |
| J3 | 4 | SPI to slave (MOSI, MISO, SCK, CS) |
| J4 | 2 | PWM output (Wiper, GND) |

## Wiring Notes

### Power

1. Connect 12V supply to AMS1117 input and LM358 pin 8
2. Connect AMS1117 output (3.3V) to ESP32 3V3 pin
3. Add 100nF ceramic capacitor between ESP32 3V3 and GND pins
4. All GND connections must be common

### CAN Bus

1. MCP2515 module typically includes 8MHz crystal and TJA1050 transceiver
2. Connect CANH/CANL to vehicle CAN bus (twisted pair recommended)
3. 120Ω termination resistor may be needed at end of CAN bus

### Rotary Encoder

1. KY-040 module has built-in pull-up resistors
2. Directly connect CLK, DT, SW to ESP32 GPIOs
3. Encoder button is active LOW (pressed = GND)

### SPI to Slave

1. Use short wires (<30cm) or shielded cable for reliability
2. All 4 SPI signals required (MOSI, MISO, SCK, CS)
3. Ground wire should accompany SPI signals

### PWM Output

1. RC filter converts PWM to smooth DC voltage
2. LM358 powered from 12V to allow 5V output swing
3. Output connects to PWM controller's wiper input
4. Leave PWM controller's 5V pin disconnected

## Test Points

| Point | Expected Value | Notes |
|-------|----------------|-------|
| TP1 (3.3V rail) | 3.3V ± 0.1V | Check with no load, then with ESP32 |
| TP2 (RC filter out) | 0-3.3V | Varies with PWM duty cycle |
| TP3 (Op-amp out) | 0-5V | Should track TP2 × 1.51 |
| TP4 (MCP2515 INT) | HIGH idle | Goes LOW on CAN activity |

## Troubleshooting

| Symptom | Possible Cause | Solution |
|---------|----------------|----------|
| ESP32 won't boot | Insufficient power | Check AMS1117 output, add capacitance |
| No CAN communication | Wrong SPI pins | Verify GPIO assignments |
| Encoder skips steps | Missing pull-ups | KY-040 module should have them |
| PWM output stuck at 0V | RC filter issue | Check capacitor polarity if electrolytic |
| PWM output max ~3.5V | LM358 on 5V | Must use 12V for LM358 VCC |
