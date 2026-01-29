# Quick Start: EasyEDA to JLCPCB in 30 Minutes

This guide gets you from zero to JLCPCB order as fast as possible.

## Prerequisites

- JLCPCB account (free): https://jlcpcb.com/register
- EasyEDA uses your JLCPCB login

## Step 1: Open EasyEDA (2 min)

1. Go to https://easyeda.com
2. Log in with JLCPCB account
3. File → New → Project → "ESP32-S3-Master"

## Step 2: Create Schematic - Add Components (10 min)

### Add all components by LCSC number:

Press `Shift+F` to open part search, enter LCSC number:

| LCSC # | Component | Designator |
|--------|-----------|------------|
| C2913202 | ESP32-S3-WROOM-1-N16R8 | U1 |
| C14259 | MP2359DJ-LF-Z | U2 |
| C7950 | LM358DR | U3 |
| C7519 | USBLC6-2SC6 | U4 |
| C8678 | SS34 | D1 |
| C123769 | SMBJ15A | D2 |
| C167134 | 10uH Inductor | L1 |
| C45783 | 22uF Cap | C1, C2 |
| C1525 | 100nF Cap | C3-C6, C9, C12, C13 |
| C15850 | 10uF Cap | C7, C8, C11 |
| C28323 | 1uF Cap | C10 |
| C25741 | 33K Resistor | R1 |
| C25744 | 10K Resistor | R2-R7, R12-R14, R17-R19 |
| C25905 | 5.1K Resistor | R4, R8, R9 |
| C25792 | 47K Resistor | R15, R16 |
| C25092 | 22R Resistor | R10, R11 |
| C8463 | Screw Terminal 2P | J1 |
| C2765186 | USB-C | J2 |
| C161872 | JST-XH 7P | J3 |
| C160404 | JST-GH 4P | J4 |
| C157991 | JST-XH 5P | J5 |
| C131337 | JST-PH 3P | J6 |
| C160408 | JST-GH 6P | J7 |
| C111196 | Micro SD | J8 |
| C492405 | Header 2x7 | J9 |
| C2889983 | ARM Debug 2x5 | J10 |
| C2977595 | Header 1x6 RA | J11 |
| C318884 | Tactile Switch | SW1, SW2 |

## Step 3: Wire the Schematic (10 min)

### Power Section:
```
J1.+ → D1.A → D2.K → +12V
J1.- → GND
D1.K → GND (reverse protection)
D2.A → GND

+12V → C1 → GND
+12V → U2.VIN
U2.GND → GND
U2.SW → L1 → +3V3
+3V3 → C2 → GND
U2.FB → R1/R2 junction
R1 → +3V3
R2 → GND
```

### ESP32 Power:
```
+3V3 → U1.3V3
GND → U1.GND
+3V3 → C3,C4,C5,C6,C7 → GND (near U1)
```

### Boot Circuit:
```
+3V3 → R6 → EN → U1.EN
EN → C12 → GND
EN → SW1 → GND

+3V3 → R7 → IO0 → U1.GPIO0
IO0 → C13 → GND
IO0 → SW2 → GND
```

### USB:
```
J2.D+ → R10 → U1.GPIO20
J2.D- → R11 → U1.GPIO19
J2.CC1 → R8 → GND
J2.CC2 → R9 → GND
J2.VBUS → C11 → GND
J2.GND → GND
```

### CAN Bus:
```
J3.VCC → +3V3
J3.GND → GND
J3.CS → U1.GPIO10
J3.SO → U1.GPIO13
J3.SI → U1.GPIO11
J3.SCK → U1.GPIO12
J3.INT → U1.GPIO9

J4.CANH → (external)
J4.CANL → (external)
J4.12V → +12V
J4.GND → GND
```

### Encoder:
```
+3V3 → R17 → J5.CLK → U1.GPIO4
+3V3 → R18 → J5.DT → U1.GPIO5
+3V3 → R19 → J5.SW → U1.GPIO6
J5.VCC → +3V3
J5.GND → GND
```

### PWM Output:
```
U1.GPIO7 → R3 → Node_A
Node_A → C10 → GND
Node_A → U3.+ (pin3)
U3.VCC (pin8) → +12V
U3.GND (pin4) → GND
U3.OUT (pin1) → J6.SIG
U3.OUT → R4 → Node_B
Node_B → U3.- (pin2)
Node_B → R5 → GND
J6.GND → GND
```

### SPI Slave:
```
U1.GPIO2 → J7.MOSI
U1.GPIO3 → J7.MISO
U1.GPIO14 → J7.SCK
U1.GPIO21 → J7.CS
J7.3V3 → +3V3
J7.GND → GND
```

### SD Card:
```
+3V3 → R12 → U1.GPIO15 → J8.CS
+3V3 → R13 → U1.GPIO16 → J8.MOSI
+3V3 → R14 → U1.GPIO17 → J8.SCK
+3V3 → R15 → U1.GPIO18 → J8.MISO
+3V3 → R16 → U1.GPIO8 → J8.CD
J8.VDD → +3V3
J8.GND → GND
+3V3 → C8,C9 → GND (near J8)
```

### JTAG:
```
J10.1 → +3V3
J10.2 → U1.GPIO42
J10.3,5,9 → GND
J10.4 → U1.GPIO39
J10.6 → U1.GPIO40
J10.8 → U1.GPIO41
J10.10 → EN
```

### UART:
```
J11.1 → GND
J11.2 → +3V3
J11.3 → U1.GPIO43
J11.4 → U1.GPIO44
J11.5 → EN
J11.6 → IO0
```

## Step 4: Convert to PCB (2 min)

1. Click **Design → Convert to PCB**
2. Board size: **75mm × 55mm**
3. Layers: **2**

## Step 5: Setup PCB (3 min)

### Design Rules:
1. **Design → Design Rules**
2. Clearance: 0.2mm
3. Track: 0.25mm min
4. Via: 0.6mm / 0.3mm drill

### Board Outline:
1. Select **Board Outline** tool
2. Draw 75×55mm rectangle
3. Round corners: 2mm

### Mounting Holes:
1. Search footprint: "Mounting Hole 3.2mm"
2. Place at: (5,5), (70,5), (5,50), (70,50)

## Step 6: Place Components (3 min)

Drag components to approximate positions:

```
┌────────────────────────────────────────────────┐
│  [USB-C]        [ESP32-S3]      [MCP] [CAN]    │
│                                                │
│  [12V]    [Buck]              [SD]   [JTAG]   │
│                                                │
│  [PWM][SPI]  [Expansion]  [Encoder] [UART]    │
└────────────────────────────────────────────────┘
```

## Step 7: Auto-Route (2 min)

1. Click **Route → Auto Route**
2. Let it complete
3. Review results

## Step 8: Add Ground Plane (1 min)

1. Select **Copper Area** tool
2. Draw rectangle covering board on **Bottom Layer**
3. Set net: **GND**

## Step 9: Check Design (1 min)

1. **Design → Check DRC**
2. Fix any errors
3. Common fixes: move overlapping components, widen traces

## Step 10: Order from JLCPCB (2 min)

1. Click **Fabrication → PCB Fabrication File**
2. Click **Order at JLCPCB**
3. Options:
   - Layers: 2
   - Qty: 5
   - Thickness: 1.6mm
   - Color: Green (or your choice)
   - Surface: HASL Lead-Free

4. **Optional SMT Assembly:**
   - Enable SMT Assembly
   - Side: Top
   - BOM and CPL auto-generated

5. Add to cart → Checkout

## Total Cost Estimate

| Item | Cost |
|------|------|
| 5× PCBs | $5-8 |
| SMT Assembly | $30-50 |
| Components | $20-30 |
| Shipping | $15-20 |
| **Total** | **$70-110** |

## Delivery Time

- PCB only: 7-15 days
- With assembly: 10-20 days
- Express shipping available

---

## Troubleshooting

**Part not found:** Try searching by manufacturer part number instead of LCSC

**ERC errors:** Add power flags to +3V3, +12V, GND nets

**DRC errors:** Increase clearance or move components apart

**Auto-route fails:** Route power traces manually first, then auto-route signals

---

## Files Reference

- `BOM_EasyEDA.csv` - Complete parts list
- `schematic_netlist.txt` - All connections
- `board_outline.json` - PCB dimensions
- `README.md` - Detailed instructions
