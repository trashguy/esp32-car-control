# ESP32-S3 Master PCB - KiCad Project

Custom PCB for the ESP32 Car Control master board. Designed for JLCPCB manufacturing and assembly.

## Project Files

| File | Description |
|------|-------------|
| `master-pcb.kicad_pro` | KiCad 7 project file |
| `master-pcb.kicad_sch` | Schematic (template - needs component placement) |
| `master-pcb.kicad_pcb` | PCB layout with board outline and mounting holes |
| `BOM_JLCPCB.csv` | Bill of Materials for JLCPCB assembly |
| `CPL_JLCPCB.csv` | Component Placement List for SMT assembly |

## Board Specifications

- **Dimensions:** 75mm × 55mm
- **Layers:** 2 (Top copper, Bottom copper)
- **Thickness:** 1.6mm FR4
- **Copper Weight:** 1oz (35µm)
- **Surface Finish:** HASL (lead-free) or ENIG
- **Solder Mask:** Green (or color of choice)
- **Silkscreen:** White

## Design Rules (JLCPCB Compatible)

| Parameter | Value |
|-----------|-------|
| Min trace width | 0.2mm (8mil) |
| Min trace spacing | 0.2mm (8mil) |
| Min via diameter | 0.5mm |
| Min via drill | 0.3mm |
| Min hole size | 0.3mm |
| Min annular ring | 0.1mm |
| Edge clearance | 0.3mm |

## How to Complete the Design

### Step 1: Install Required Libraries

Open KiCad and install these libraries via Library Manager:

1. **ESP32-S3-WROOM-1** - Search "ESP32" in symbol/footprint libraries
2. **USB_C_Receptacle** - In Connector library
3. **JST connectors** - In Connector_JST library

Or download from:
- https://github.com/espressif/kicad-libraries (ESP32 modules)
- https://github.com/Digi-Key/digikey-kicad-library

### Step 2: Complete the Schematic

1. Open `master-pcb.kicad_sch` in KiCad
2. Add components from the libraries:
   - U1: ESP32-S3-WROOM-1-N16R8
   - U2: MP2359DJ-LF-Z (buck converter)
   - U3: LM358 (op-amp)
   - U4: USBLC6-2SC6 (USB ESD)
   - All connectors (J1-J11)
   - Passive components (R, C, L)

3. Wire according to the pin assignments in `docs/schematics/master-custom-pcb.md`

4. Run ERC (Electrical Rules Check)

### Step 3: Assign Footprints

1. Open the schematic
2. Tools → Assign Footprints
3. Match each component to its footprint:

| Component | Footprint |
|-----------|-----------|
| ESP32-S3-WROOM-1 | RF_Module:ESP32-S3-WROOM-1 |
| MP2359 | Package_TO_SOT_SMD:SOT-23-6 |
| LM358 | Package_SO:SOIC-8 |
| USB-C | Connector_USB:USB_C_Receptacle_GCT_USB4110 |
| JST-XH | Connector_JST:JST_XH_B*B-XH-A_1x*_P2.50mm_Vertical |
| JST-GH | Connector_JST:JST_GH_SM*B-GHS-TB_1x*_P1.25mm_Horizontal |
| JST-PH | Connector_JST:JST_PH_B*B-PH-K-S_1x*_P2.00mm_Vertical |
| 0402 R/C | Resistor_SMD:R_0402_1005Metric |
| 0805 C | Capacitor_SMD:C_0805_2012Metric |

### Step 4: PCB Layout

1. Open `master-pcb.kicad_pcb`
2. Import netlist from schematic (Tools → Update PCB from Schematic)
3. Place components following the layout guide in the silkscreen
4. Route traces:
   - Power traces (3V3, 12V, GND): 0.5-0.8mm width
   - Signal traces: 0.3mm width
   - USB D+/D-: Match lengths, 90Ω differential
5. Add ground fill on bottom layer
6. Run DRC (Design Rules Check)

### Step 5: Generate Manufacturing Files

1. File → Fabrication Outputs → Gerbers
2. Select output directory: `gerbers/`
3. Layers to include:
   - F.Cu, B.Cu (copper)
   - F.SilkS, B.SilkS (silkscreen)
   - F.Mask, B.Mask (solder mask)
   - Edge.Cuts (board outline)
4. Generate drill files (Excellon format)
5. Zip the gerbers folder

## Ordering from JLCPCB

### PCB Only (you solder components)

1. Go to https://jlcpcb.com
2. Click "Quote Now"
3. Upload gerbers.zip
4. Select options:
   - Layers: 2
   - Dimensions: 75 × 55 mm
   - PCB Qty: 5 (minimum)
   - Thickness: 1.6mm
   - Surface Finish: HASL lead-free
5. Add to cart and checkout

### PCB + SMT Assembly

1. Upload gerbers.zip
2. Enable "SMT Assembly"
3. Select assembly side: Top
4. Upload BOM: `BOM_JLCPCB.csv`
5. Upload CPL: `CPL_JLCPCB.csv` (generate from KiCad)
6. Review component placement
7. Some parts may need manual sourcing (ESP32 module, JST connectors)

### Cost Estimate (2026 prices, approximate)

| Item | Qty 5 | Qty 10 |
|------|-------|--------|
| PCB only | $8-12 | $10-15 |
| SMT assembly | $30-50 | $40-60 |
| Components | $15-25 | $25-40 |
| Shipping | $15-30 | $15-30 |
| **Total** | **$70-120** | **$90-145** |

## Components to Hand-Solder

These components are typically not available in JLCPCB's parts library:

1. **ESP32-S3-WROOM-1-N16R8** - Available but may need extended parts
2. **Micro SD slot** - Often need to source separately
3. **USB-C connector** - Check availability
4. **JST connectors** - May need to solder manually

## Testing After Assembly

1. **Visual inspection** - Check for solder bridges, missing components
2. **Power test** - Apply 12V, verify 3.3V output
3. **USB test** - Connect USB-C, check if ESP32 is detected
4. **Program test** - Upload test firmware via USB
5. **Peripheral test** - Test each connector/interface

## Schematic Reference

See `docs/schematics/master-custom-pcb.md` for:
- Complete circuit diagrams
- Pin assignments
- Component values
- Wiring notes

## Connector Pinouts Quick Reference

### J4 - CAN Bus (JST-GH 4P)
1: CANH, 2: CANL, 3: 12V, 4: GND

### J5 - Encoder (JST-XH 5P)
1: CLK, 2: DT, 3: SW, 4: 3V3, 5: GND

### J6 - PWM Out (JST-PH 3P)
1: SIG (0-5V), 2: 5V, 3: GND

### J7 - SPI Slave (JST-GH 6P)
1: MOSI, 2: MISO, 3: SCK, 4: CS, 5: 3V3, 6: GND

### J10 - JTAG (ARM 10P)
1: 3V3, 2: TMS, 3: GND, 4: TCK, 5: GND, 6: TDO, 7: NC, 8: TDI, 9: GND, 10: RST

### J11 - UART (1x6)
1: GND, 2: 3V3, 3: TX, 4: RX, 5: EN, 6: IO0
