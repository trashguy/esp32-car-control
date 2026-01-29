# ESP32-S3 Master PCB - EasyEDA Project

Step-by-step guide to create the custom PCB in EasyEDA and order from JLCPCB.

## Quick Start

1. Go to https://easyeda.com and create account (or use JLCPCB account)
2. Create new project: "ESP32-S3 Master PCB"
3. Follow the instructions below to create schematic and PCB

## Project Files

| File | Purpose |
|------|---------|
| `BOM_EasyEDA.csv` | Component list with LCSC part numbers |
| `schematic_netlist.txt` | All connections between components |
| `board_outline.json` | PCB outline coordinates |
| `README.md` | This guide |

## Board Specifications

- **Size:** 75mm × 55mm
- **Layers:** 2
- **Thickness:** 1.6mm
- **Copper:** 1oz
- **Surface:** HASL Lead-Free

---

## Step 1: Create New Project

1. Open EasyEDA: https://easyeda.com/editor
2. File → New → Project
3. Name: "ESP32-S3-Master-PCB"
4. Click Create

---

## Step 2: Create Schematic - Power Supply

### 2.1 Add 12V Input and Protection

1. **Search and place components** (use LCSC part numbers):
   - `C8463` - Screw Terminal J1
   - `C8678` - SS34 Schottky D1
   - `C123769` - SMBJ15A TVS D2

2. **Wire:**
   ```
   J1.1 (+12V) → D1.Anode → D2.Cathode → +12V net
   J1.2 (GND) → D1.Cathode (for reverse polarity)
   D2.Anode → GND
   ```

### 2.2 Add Buck Converter (12V → 3.3V)

1. **Place components:**
   - `C14259` - MP2359DJ-LF-Z (U2)
   - `C167134` - 10uH Inductor (L1)
   - `C45783` - 22uF Input Cap (C1)
   - `C45783` - 22uF Output Cap (C2)
   - `C25741` - 33K Resistor (R1)
   - `C25744` - 10K Resistor (R2)

2. **Wire:**
   ```
   +12V → C1.1 → U2.VIN
   C1.2 → GND
   U2.GND → GND
   U2.SW → L1.1
   L1.2 → C2.1 → +3V3 net
   C2.2 → GND
   U2.FB → R1/R2 junction
   R1.1 → +3V3
   R2.2 → GND
   ```

---

## Step 3: Create Schematic - ESP32-S3 Module

### 3.1 Add ESP32-S3-WROOM-1

1. **Search:** `C2913202` or search "ESP32-S3-WROOM-1"
2. **Place U1** in center of schematic

### 3.2 Add Decoupling Capacitors

1. **Place near 3V3 pins:**
   - 4× `C1525` - 100nF (C3-C6)
   - 1× `C15850` - 10uF (C7)

2. **Wire all to 3V3 and GND**

### 3.3 Add Boot/Reset Circuit

1. **Place:**
   - 2× `C25744` - 10K Resistors (R6, R7)
   - 2× `C1525` - 100nF Capacitors (C12, C13)
   - 2× `C318884` - Tactile Switches (SW1, SW2)

2. **Wire:**
   ```
   Reset Circuit:
   +3V3 → R6 → EN pin
   EN → C12 → GND
   EN → SW1 → GND

   Boot Circuit:
   +3V3 → R7 → GPIO0
   GPIO0 → C13 → GND
   GPIO0 → SW2 → GND
   ```

---

## Step 4: Create Schematic - USB-C Interface

### 4.1 Add USB Components

1. **Place:**
   - `C2765186` - USB-C Connector (J2)
   - `C7519` - USBLC6-2SC6 ESD (U4)
   - 2× `C25905` - 5.1K Resistors (R8, R9) for CC
   - 2× `C25092` - 22R Resistors (R10, R11) for D+/D-
   - `C15850` - 10uF Capacitor (C11)

2. **Wire:**
   ```
   J2.VBUS → C11 → GND
   J2.CC1 → R8 → GND
   J2.CC2 → R9 → GND
   J2.D+ → R10 → U4.IO1 → ESP32 GPIO20
   J2.D- → R11 → U4.IO2 → ESP32 GPIO19
   U4.VCC → +3V3
   U4.GND → GND
   J2.GND → GND
   ```

---

## Step 5: Create Schematic - CAN Bus Interface

### 5.1 Add MCP2515 Module Header

1. **Place:**
   - `C161872` - JST-XH 7-pin (J3)
   - `C160404` - JST-GH 4-pin (J4)

2. **Wire to ESP32:**
   ```
   J3.1 (VCC) → +3V3
   J3.2 (GND) → GND
   J3.3 (CS) → GPIO10
   J3.4 (SO/MISO) → GPIO13
   J3.5 (SI/MOSI) → GPIO11
   J3.6 (SCK) → GPIO12
   J3.7 (INT) → GPIO9

   J4.1 (CANH) → External CAN
   J4.2 (CANL) → External CAN
   J4.3 (12V) → +12V (passthrough)
   J4.4 (GND) → GND
   ```

---

## Step 6: Create Schematic - Rotary Encoder

### 6.1 Add Encoder Interface

1. **Place:**
   - `C157991` - JST-XH 5-pin (J5)
   - 3× `C25744` - 10K Pull-up Resistors (R17-R19)

2. **Wire:**
   ```
   J5.1 (CLK) → R17 → +3V3, also → GPIO4
   J5.2 (DT) → R18 → +3V3, also → GPIO5
   J5.3 (SW) → R19 → +3V3, also → GPIO6
   J5.4 (VCC) → +3V3
   J5.5 (GND) → GND
   ```

---

## Step 7: Create Schematic - PWM Output

### 7.1 Add RC Filter and Op-Amp

1. **Place:**
   - `C7950` - LM358 Op-Amp (U3)
   - `C25744` - 10K RC Filter Resistor (R3)
   - `C28323` - 1uF RC Filter Cap (C10)
   - `C25905` - 5.1K Feedback Resistor (R4)
   - `C25744` - 10K Gain Resistor (R5)
   - `C131337` - JST-PH 3-pin (J6)

2. **Wire:**
   ```
   GPIO7 → R3 → Node_A
   Node_A → C10 → GND
   Node_A → U3.IN+ (pin 3)
   U3.VCC (pin 8) → +12V
   U3.GND (pin 4) → GND
   U3.OUT (pin 1) → R4 → Node_B
   Node_B → R5 → GND
   Node_B → U3.IN- (pin 2)
   U3.OUT → J6.1 (SIG)
   +5V → J6.2 (optional 5V ref)
   GND → J6.3
   ```

---

## Step 8: Create Schematic - SPI Slave Communication

1. **Place:**
   - `C160408` - JST-GH 6-pin (J7)

2. **Wire:**
   ```
   J7.1 (MOSI) → GPIO2
   J7.2 (MISO) → GPIO3
   J7.3 (SCK) → GPIO14
   J7.4 (CS) → GPIO21
   J7.5 (3V3) → +3V3
   J7.6 (GND) → GND
   ```

---

## Step 9: Create Schematic - SD Card

### 9.1 Add Micro SD Slot

1. **Place:**
   - `C111196` - Micro SD Slot (J8)
   - `C15850` - 10uF Decoupling (C8)
   - `C1525` - 100nF Decoupling (C9)
   - 4× `C25744` - 10K Pull-ups (R12-R14, reuse)
   - 2× `C25792` - 47K Pull-ups (R15, R16)

2. **Wire:**
   ```
   J8.VDD → C8 → GND, also → C9 → GND, also → +3V3
   J8.VSS → GND
   J8.CS → R12 → +3V3, also → GPIO15
   J8.MOSI → R13 → +3V3, also → GPIO16
   J8.SCK → R14 → +3V3, also → GPIO17
   J8.MISO → R15 → +3V3, also → GPIO18
   J8.CD → R16 → +3V3, also → GPIO8
   ```

---

## Step 10: Create Schematic - Debug Interfaces

### 10.1 Add JTAG Header

1. **Place:**
   - `C2889983` - ARM Debug 2x5 1.27mm (J10)

2. **Wire:**
   ```
   J10.1 → +3V3
   J10.2 (TMS) → GPIO42
   J10.3 → GND
   J10.4 (TCK) → GPIO39
   J10.5 → GND
   J10.6 (TDO) → GPIO40
   J10.7 → NC
   J10.8 (TDI) → GPIO41
   J10.9 → GND
   J10.10 (RST) → EN
   ```

### 10.2 Add UART Header

1. **Place:**
   - `C2977595` - Header 1x6 Right Angle (J11)

2. **Wire:**
   ```
   J11.1 → GND
   J11.2 → +3V3
   J11.3 (TX) → GPIO43
   J11.4 (RX) → GPIO44
   J11.5 (EN) → EN
   J11.6 (IO0) → GPIO0
   ```

---

## Step 11: Create Schematic - Expansion Header

1. **Place:**
   - `C492405` - Header 2x7 2.54mm (J9)

2. **Wire available GPIOs and power to header**

---

## Step 12: Run ERC (Electrical Rules Check)

1. Click **Design → Check ERC**
2. Fix any errors (unconnected pins, etc.)
3. All power pins should be connected
4. Add "No Connect" flags to unused ESP32 pins

---

## Step 13: Create PCB

### 13.1 Convert Schematic to PCB

1. Click **Design → Convert to PCB**
2. Set board size: 75mm × 55mm
3. Set layers: 2

### 13.2 Set Design Rules (JLCPCB Compatible)

1. **Design → Design Rules**
2. Set:
   - Clearance: 0.2mm
   - Track Width: 0.25mm min, 0.5mm for power
   - Via: 0.6mm diameter, 0.3mm drill
   - Hole: 0.3mm min

### 13.3 Draw Board Outline

1. Select **Edge.Cuts** layer
2. Draw rectangle: 75mm × 55mm
3. Round corners: 2mm radius
4. Or import coordinates from `board_outline.json`

### 13.4 Place Components

Suggested placement (see board diagram in main README):

```
Top-left:     USB-C (J2)
Left edge:    12V Power (J1)
Center:       ESP32-S3 Module (U1)
Top-right:    MCP2515 Header (J3), CAN connector (J4)
Right:        SD Card (J8), JTAG (J10)
Bottom-left:  PWM (J6), SPI Slave (J7)
Bottom:       Expansion (J9)
Bottom-right: Encoder (J5), UART (J11)
```

### 13.5 Add Mounting Holes

1. Place 4× mounting holes (3.2mm) at:
   - (5, 5)
   - (70, 5)
   - (5, 50)
   - (70, 50)

### 13.6 Route Traces

1. **Auto-route** for initial routing (Tools → Auto Route)
2. **Manual cleanup:**
   - Power traces: 0.5-0.8mm width
   - Signal traces: 0.25-0.3mm
   - USB D+/D-: Match lengths

### 13.7 Add Ground Plane

1. Select **Bottom Copper** layer
2. Draw copper zone covering entire board
3. Set net: GND

### 13.8 Add Antenna Keep-Out

1. On **Top Copper** layer
2. Draw keep-out zone around ESP32 antenna area
3. No copper within 10mm of antenna end

---

## Step 14: Run DRC

1. Click **Design → Check DRC**
2. Fix all errors
3. Review warnings

---

## Step 15: Order from JLCPCB

### 15.1 Generate Manufacturing Files

1. Click **Fabrication → PCB Fabrication File (Gerber)**
2. Files are automatically compatible with JLCPCB

### 15.2 Order PCB

1. Click **Fabrication → One-Click Order PCB/SMT**
2. Or go to https://jlcpcb.com
3. Upload Gerber files
4. Select options:
   - Layers: 2
   - Size: 75 × 55 mm
   - Quantity: 5+
   - Thickness: 1.6mm
   - Surface: HASL Lead-Free

### 15.3 Order with SMT Assembly (Optional)

1. Enable **SMT Assembly**
2. Select **Top Side**
3. Upload BOM: `BOM_EasyEDA.csv`
4. EasyEDA auto-generates placement file
5. Review component positions
6. Note: Some parts may need manual sourcing

---

## Estimated Costs

| Service | Qty 5 | Qty 10 |
|---------|-------|--------|
| PCB Only | $5-10 | $8-15 |
| SMT Assembly | $25-40 | $35-50 |
| Components | $20-35 | $35-55 |
| Shipping | $15-25 | $15-25 |
| **Total** | **$65-110** | **$95-145** |

---

## Testing After Assembly

1. **Visual Check** - Inspect for solder bridges
2. **Power Test** - Apply 12V, measure 3.3V output
3. **USB Test** - Connect USB-C, check device detection
4. **Flash Test** - Upload firmware via USB or UART
5. **Peripheral Test** - Test each connector

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| No 3.3V output | Check buck converter, inductor orientation |
| USB not detected | Check CC resistors, D+/D- routing |
| ESP32 won't boot | Check EN/IO0 pull-ups, reset circuit |
| SD card fails | Check pull-ups, decoupling caps |
| CAN not working | Check MCP2515 module connection, INT pin |

---

## Reference Documents

- `docs/schematics/master-custom-pcb.md` - Full schematic details
- `include/shared/config.h` - Pin definitions
- ESP32-S3 Datasheet: https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf
- ESP32-S3-WROOM-1 Datasheet: https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf
