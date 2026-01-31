#!/usr/bin/env python3
"""Generate KiCad netlist file for ESP32-S3 Master PCB"""

from datetime import datetime

# Parse the netlist from schematic_netlist.txt format
NETLIST_DATA = """
# POWER NETS
+12V: J1.1, D1.A, U2.VIN, U3.8, D2.K, J4.3
+3V3: U2.OUT, U1.3V3, U1.3V3_2, J3.1, J7.5, J8.VDD, J9.3V3, J10.1, J11.2, J14.4, J15.4, J16.4, J17.4, J18.4, C3.1, C4.1, C5.1, C6.1, C7.1, C8.1, C9.1, C14.1, C16.1, R6.1, R7.1, R12.1, R13.1, R14.1, R15.1, R16.1, R21.1, R22.1, R24.1, R25.1, R26.1, U6.8, U7.9, U7.18, R27.1
+5V: J6.2
GND: J1.2, D1.K, D2.A, U2.GND, U3.4, U4.2, U5.3, U6.1, U6.4, U7.10, U7.15, U7.16, U7.17, U1.GND, U1.GND_2, U1.GND_3, J2.GND, J3.2, J4.4, J6.3, J7.6, J8.GND, J9.GND, J10.3, J10.5, J10.9, J11.1, J12.2, J13.2, J14.5, J15.5, J16.5, J17.5, J18.5, C1.2, C2.2, C3.2, C4.2, C5.2, C6.2, C7.2, C8.2, C9.2, C10.2, C11.2, C12.2, C13.2, C14.2, C15.2, C16.2, R2.2, R5.2, R8.2, R9.2, D4.2, SW1.2, SW2.2, J19.2, C17.2, C18.2
VBUS: J2.VBUS, C11.1, U4.5

# USB INTERFACE
USB_DP: J2.D+, R10.1
USB_DP_ESP: R10.2, U1.GPIO20, U4.1
USB_DM: J2.D-, R11.1
USB_DM_ESP: R11.2, U1.GPIO19, U4.3
CC1: J2.CC1, R8.1
CC2: J2.CC2, R9.1

# POWER SUPPLY
SW_NODE: U2.SW, L1.1
FB_NODE: U2.FB, R1.2, R2.1
VOUT_PRE: L1.2, C2.1, R1.1

# CAN CONTROLLER
MCP_SCK: U1.GPIO12, J3.6
MCP_MOSI: U1.GPIO11, J3.5
MCP_MISO: U1.GPIO13, J3.4
MCP_CS: U1.GPIO10, J3.3
MCP_INT: U1.GPIO9, J3.7
CANH: J4.1
CANL: J4.2

# PWM OUTPUT
PWM_RAW: U1.GPIO7, R3.1
PWM_FILTERED: R3.2, C10.1, U3.3
PWM_AMPLIFIED: U3.1, R4.1, J6.1
OPAMP_FB: R4.2, R5.1, U3.2

# SPI SLAVE
COMM_MOSI: U1.GPIO2, J7.1
COMM_MISO: U1.GPIO3, J7.2
COMM_SCK: U1.GPIO14, J7.3
COMM_CS: U1.GPIO21, R26.2, J7.4

# SD CARD
SD_CS: U1.GPIO15, R12.2, J8.CS
SD_MOSI: U1.GPIO16, R13.2, J8.MOSI
SD_SCK: U1.GPIO17, R14.2, J8.SCK
SD_MISO: U1.GPIO18, R15.2, J8.MISO
SD_CD: U1.GPIO8, R16.2, J8.CD

# JTAG
JTAG_TCK: U1.GPIO39, J10.4
JTAG_TDO: U1.GPIO40, J10.6
JTAG_TDI: U1.GPIO41, J10.8
JTAG_TMS: U1.GPIO42, J10.2

# UART
UART_TX: U1.GPIO43, J11.3
UART_RX: U1.GPIO44, J11.4

# BOOT/RESET
EN: U1.EN, R6.2, C12.1, SW1.1, J10.10, J11.5
IO0: U1.GPIO0, R7.2, C13.1, SW2.1, J11.6

# RPM INPUT
RPM_12V_IN: J12.1, R20.1
RPM_LED_CATHODE: R20.2, U5.1
RPM_PROTECTION: D3.A, J12.2
RPM_LED_ANODE: U5.2, D3.K
RPM_COLLECTOR: U5.4, R21.2, U1.GPIO1
RPM_EMITTER: U5.3

# VSS INPUT
VSS_VR_POS: J13.1, U6.3, C15.1, D4.1
VSS_VR_NEG: J13.2, U6.4
VSS_RSET: U6.6, R23.1
VSS_RSET_GND: R23.2
VSS_OUTPUT: U6.7, R22.2, U1.GPIO38
VSS_VCC: U6.8, C14.1

# I2C / MCP23017
I2C_SDA: U1.GPIO47, U7.13, R24.2
I2C_SCL: U1.GPIO48, U7.12, R25.2
MCP23017_INT: U1.GPIO45, U7.20
MCP23017_A0: U7.15
MCP23017_A1: U7.16
MCP23017_A2: U7.17
MCP23017_RESET: U7.18
MCP23017_VCC: U7.9, C16.1

# ENCODERS
ENC1_CLK: U7.21, J14.1
ENC1_DT: U7.22, J14.2
ENC1_SW: U7.23, J14.3
ENC2_CLK: U7.24, J15.1
ENC2_DT: U7.25, J15.2
ENC2_SW: U7.26, J15.3
ENC3_CLK: U7.27, J16.1
ENC3_DT: U7.28, J16.2
ENC3_SW: U7.1, J16.3
ENC4_CLK: U7.2, J17.1
ENC4_DT: U7.3, J17.2
ENC4_SW: U7.4, J17.3
ENC5_CLK: U7.5, J18.1
ENC5_DT: U7.6, J18.2
ENC5_SW: U7.7, J18.3

# WATER TEMPERATURE INPUT (GM LS1 NTC Sensor on GPIO4)
# Voltage divider with 1K pull-up, ESD protection, RC filter
WATER_TEMP_PULLUP: R27.1, C17.1
WATER_TEMP_SENSE: R27.2, C17.2, J19.1, R28.1
WATER_TEMP_PROTECTED: R28.2, D5.1
WATER_TEMP_FILTERED: D5.2, R29.1
WATER_TEMP_IN: R29.2, C18.1, U1.GPIO4

# INPUT CAPS
C1_POS: C1.1, U2.VIN
"""

# Component definitions
COMPONENTS = {
    "U1": {"value": "ESP32-S3-WROOM-1-N16R8", "footprint": "ESP32-S3-WROOM-1"},
    "U2": {"value": "MP2359DJ-LF-Z", "footprint": "SOT-23-6"},
    "U3": {"value": "LM358DR", "footprint": "SOIC-8"},
    "U4": {"value": "USBLC6-2SC6", "footprint": "SOT-23-6"},
    "U5": {"value": "PC817C", "footprint": "DIP-4"},
    "U6": {"value": "LM1815M", "footprint": "SOIC-8"},
    "U7": {"value": "MCP23017-E/SO", "footprint": "SOIC-28"},
    "D1": {"value": "SS34", "footprint": "SMA"},
    "D2": {"value": "SMBJ15A", "footprint": "SMB"},
    "D3": {"value": "1N4148W", "footprint": "SOD-123"},
    "D4": {"value": "P6KE33CA", "footprint": "SMB"},
    "D5": {"value": "BZT52C3V3", "footprint": "SOD-123"},
    "L1": {"value": "10uH", "footprint": "IND-SMD_4x4"},
    "SW1": {"value": "Reset", "footprint": "SW-SMD-6x6"},
    "SW2": {"value": "Boot", "footprint": "SW-SMD-6x6"},
    "J1": {"value": "Screw_Terminal_2P", "footprint": "TerminalBlock_5.08mm_2P"},
    "J2": {"value": "USB-C", "footprint": "USB-C-SMD-16P"},
    "J3": {"value": "JST-XH_7P", "footprint": "JST-XH-7A"},
    "J4": {"value": "JST-GH_4P", "footprint": "JST-GH-4P-SMD"},
    "J6": {"value": "JST-PH_3P", "footprint": "JST-PH-3A"},
    "J7": {"value": "JST-GH_6P", "footprint": "JST-GH-6P-SMD"},
    "J8": {"value": "Micro_SD", "footprint": "MICRO-SD-PUSH"},
    "J9": {"value": "Header_2x7", "footprint": "HDR-2x7-2.54"},
    "J10": {"value": "ARM_Debug_2x5", "footprint": "HDR-2x5-1.27"},
    "J11": {"value": "Header_1x6_RA", "footprint": "HDR-1x6-2.54-RA"},
    "J12": {"value": "JST-PH_2P", "footprint": "JST-PH-2A"},
    "J13": {"value": "JST-PH_2P", "footprint": "JST-PH-2A"},
    "J14": {"value": "JST-XH_5P", "footprint": "JST-XH-5A"},
    "J15": {"value": "JST-XH_5P", "footprint": "JST-XH-5A"},
    "J16": {"value": "JST-XH_5P", "footprint": "JST-XH-5A"},
    "J17": {"value": "JST-XH_5P", "footprint": "JST-XH-5A"},
    "J18": {"value": "JST-XH_5P", "footprint": "JST-XH-5A"},
    "J19": {"value": "JST-PH_2P", "footprint": "JST-PH-2A"},
}

# Add capacitors
cap_values = {1: "22uF", 2: "22uF", 7: "10uF", 8: "10uF", 10: "1uF", 11: "10uF", 15: "1nF"}
for i in range(1, 19):
    COMPONENTS[f"C{i}"] = {"value": cap_values.get(i, "100nF"), "footprint": "0402" if i not in [1,2,7,8,10,11] else "0805"}

# Add resistors
resistor_values = {
    1: "33K", 2: "10K", 3: "10K", 4: "5.1K", 5: "10K", 6: "10K", 7: "10K",
    8: "5.1K", 9: "5.1K", 10: "22R", 11: "22R", 12: "10K", 13: "10K", 14: "10K",
    15: "47K", 16: "47K", 20: "2.2K", 21: "10K", 22: "10K", 23: "200K", 24: "10K",
    25: "10K", 26: "10K", 27: "1K", 28: "10K", 29: "1K"
}
for i, val in resistor_values.items():
    COMPONENTS[f"R{i}"] = {"value": val, "footprint": "0402"}


def parse_netlist():
    """Parse the netlist data into a dictionary"""
    nets = {}
    for line in NETLIST_DATA.strip().split('\n'):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        if ':' in line:
            parts = line.split(':', 1)
            net_name = parts[0].strip()
            pins = [p.strip() for p in parts[1].split(',')]
            nets[net_name] = pins
    return nets


def normalize_pin(pin_str):
    """Convert pin notation to ref and pin number"""
    # Handle special cases like U1.GPIO20 -> U1, GPIO20
    # and J2.D+ -> J2, D+
    if '.' in pin_str:
        ref, pin = pin_str.split('.', 1)
        return ref.strip(), pin.strip()
    return pin_str, "1"


def generate_kicad_netlist():
    """Generate KiCad format netlist"""
    nets = parse_netlist()
    
    output = []
    output.append("(export (version D)")
    output.append(f'  (design')
    output.append(f'    (source "ESP32-S3-Master-PCB.kicad_sch")')
    output.append(f'    (date "{datetime.now().strftime("%Y-%m-%d %H:%M:%S")}")')
    output.append(f'    (tool "Custom Python Generator")')
    output.append(f'  )')
    
    # Components section
    output.append('  (components')
    for ref, comp in sorted(COMPONENTS.items()):
        output.append(f'    (comp (ref {ref})')
        output.append(f'      (value "{comp["value"]}")')
        output.append(f'      (footprint "{comp["footprint"]}")')
        output.append(f'    )')
    output.append('  )')
    
    # Libparts section (simplified)
    output.append('  (libparts')
    output.append('  )')
    
    # Libraries section
    output.append('  (libraries')
    output.append('  )')
    
    # Nets section
    output.append('  (nets')
    net_code = 1
    for net_name, pins in sorted(nets.items()):
        output.append(f'    (net (code {net_code}) (name "{net_name}")')
        for pin_str in pins:
            ref, pin = normalize_pin(pin_str)
            if ref in COMPONENTS:
                output.append(f'      (node (ref {ref}) (pin {pin}))')
        output.append('    )')
        net_code += 1
    output.append('  )')
    
    output.append(')')
    
    return '\n'.join(output)


def generate_orcad_netlist():
    """Generate OrcAD/Allegro format netlist (simpler, more widely compatible)"""
    nets = parse_netlist()
    
    output = []
    output.append("* ESP32-S3 Master PCB Netlist")
    output.append(f"* Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    output.append("*")
    output.append("")
    
    # Components
    output.append("*COMP_LIST")
    for ref, comp in sorted(COMPONENTS.items()):
        output.append(f'{ref} "{comp["value"]}" "{comp["footprint"]}"')
    output.append("")
    
    # Nets
    output.append("*NET_LIST")
    for net_name, pins in sorted(nets.items()):
        pin_list = []
        for pin_str in pins:
            ref, pin = normalize_pin(pin_str)
            if ref in COMPONENTS:
                pin_list.append(f"{ref}.{pin}")
        if pin_list:
            output.append(f"{net_name}: {', '.join(pin_list)}")
    output.append("")
    output.append("*END")
    
    return '\n'.join(output)


if __name__ == "__main__":
    # Generate KiCad netlist
    kicad_netlist = generate_kicad_netlist()
    with open("/home/trashguy/Projects/esp32-car-control/hardware/master-pcb-easyeda/kicad/ESP32-S3-Master-PCB.net", "w") as f:
        f.write(kicad_netlist)
    print("Generated ESP32-S3-Master-PCB.net (KiCad format)")
    
    # Generate simple text netlist
    simple_netlist = generate_orcad_netlist()
    with open("/home/trashguy/Projects/esp32-car-control/hardware/master-pcb-easyeda/kicad/ESP32-S3-Master-PCB_netlist.txt", "w") as f:
        f.write(simple_netlist)
    print("Generated ESP32-S3-Master-PCB_netlist.txt (simple format)")
    
    print(f"\nComponents: {len(COMPONENTS)}")
    nets = parse_netlist()
    print(f"Nets: {len(nets)}")
