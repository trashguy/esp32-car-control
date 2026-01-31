#!/usr/bin/env python3
"""Generate KiCad schematic file for ESP32-S3 Master PCB"""

import uuid

def gen_uuid():
    return str(uuid.uuid4())

# Component definitions with pin counts
COMPONENTS = {
    "U1": {"name": "ESP32-S3-WROOM-1-N16R8", "pins": 44, "lcsc": "C2913202"},
    "U2": {"name": "MP2359DJ-LF-Z", "pins": 6, "lcsc": "C14259"},
    "U3": {"name": "LM358DR", "pins": 8, "lcsc": "C7950"},
    "U4": {"name": "USBLC6-2SC6", "pins": 6, "lcsc": "C7519"},
    "U5": {"name": "PC817C", "pins": 4, "lcsc": "C66463"},
    "U6": {"name": "LM1815M", "pins": 8, "lcsc": "C129587"},
    "U7": {"name": "MCP23017-E/SO", "pins": 28, "lcsc": "C47023"},
    "D1": {"name": "SS34", "pins": 2, "lcsc": "C8678"},
    "D2": {"name": "SMBJ15A", "pins": 2, "lcsc": "C123769"},
    "D3": {"name": "1N4148W", "pins": 2, "lcsc": "C81598"},
    "D4": {"name": "P6KE33CA", "pins": 2, "lcsc": "C108380"},
    "D5": {"name": "BZT52C3V3", "pins": 2, "lcsc": "C173386"},
    "L1": {"name": "10uH", "pins": 2, "lcsc": "C167134"},
    "SW1": {"name": "Reset", "pins": 2, "lcsc": "C318884"},
    "SW2": {"name": "Boot", "pins": 2, "lcsc": "C318884"},
}

# Add capacitors C1-C18
for i in range(1, 19):
    if i in [1, 2]:
        COMPONENTS[f"C{i}"] = {"name": "22uF", "pins": 2, "lcsc": "C45783"}
    elif i in [7, 8, 11, 19]:
        COMPONENTS[f"C{i}"] = {"name": "10uF", "pins": 2, "lcsc": "C15850"}
    elif i == 10:
        COMPONENTS[f"C{i}"] = {"name": "1uF", "pins": 2, "lcsc": "C28323"}
    elif i == 15:
        COMPONENTS[f"C{i}"] = {"name": "1nF", "pins": 2, "lcsc": "C1523"}
    else:
        COMPONENTS[f"C{i}"] = {"name": "100nF", "pins": 2, "lcsc": "C1525"}

# Add resistors R1-R29 (R17-R19 skipped, R27-R29 for water temp)
resistor_values = {
    1: "33K", 2: "10K", 3: "10K", 4: "5.1K", 5: "10K", 6: "10K", 7: "10K",
    8: "5.1K", 9: "5.1K", 10: "22R", 11: "22R", 12: "10K", 13: "10K", 14: "10K",
    15: "47K", 16: "47K", 20: "2.2K", 21: "10K", 22: "10K", 23: "200K", 24: "10K",
    25: "10K", 26: "10K", 27: "1K", 28: "10K", 29: "1K"
}
for i, val in resistor_values.items():
    COMPONENTS[f"R{i}"] = {"name": val, "pins": 2, "lcsc": "C25744"}

# Add connectors
COMPONENTS["J1"] = {"name": "Screw Terminal 2P", "pins": 2, "lcsc": "C8463"}
COMPONENTS["J2"] = {"name": "USB-C", "pins": 16, "lcsc": "C2765186"}
COMPONENTS["J3"] = {"name": "JST-XH 7P", "pins": 7, "lcsc": "C161872"}
COMPONENTS["J4"] = {"name": "JST-GH 4P", "pins": 4, "lcsc": "C160404"}
COMPONENTS["J6"] = {"name": "JST-PH 3P", "pins": 3, "lcsc": "C131337"}
COMPONENTS["J7"] = {"name": "JST-GH 6P", "pins": 6, "lcsc": "C160408"}
COMPONENTS["J8"] = {"name": "Micro SD", "pins": 9, "lcsc": "C111196"}
COMPONENTS["J9"] = {"name": "Header 2x7", "pins": 14, "lcsc": "C492405"}
COMPONENTS["J10"] = {"name": "ARM Debug 2x5", "pins": 10, "lcsc": "C2889983"}
COMPONENTS["J11"] = {"name": "Header 1x6 RA", "pins": 6, "lcsc": "C2977595"}
COMPONENTS["J12"] = {"name": "JST-PH 2P", "pins": 2, "lcsc": "C131338"}
COMPONENTS["J13"] = {"name": "JST-PH 2P", "pins": 2, "lcsc": "C131338"}
for i in range(14, 19):
    COMPONENTS[f"J{i}"] = {"name": "JST-XH 5P", "pins": 5, "lcsc": "C157991"}
# J19 - Water Temperature sensor input
COMPONENTS["J19"] = {"name": "JST-PH 2P", "pins": 2, "lcsc": "C131338"}

def get_symbol_name(ref, comp):
    """Generate a unique symbol name based on component"""
    name = comp["name"]
    # Replace special characters that might cause issues
    safe_name = name.replace("/", "_").replace(" ", "_")
    return f"{safe_name}_{ref}"

def generate_lib_symbol(ref, comp):
    """Generate a library symbol definition"""
    name = comp["name"]
    sym_name = get_symbol_name(ref, comp)
    pins = comp["pins"]
    height = max(pins * 2.54, 10.16)
    
    # Extract prefix for Reference (U, R, C, D, J, L, SW)
    ref_prefix = ''.join(c for c in ref if c.isalpha())
    
    symbol = f'''
    (symbol "{sym_name}" (in_bom yes) (on_board yes)
      (property "Reference" "{ref_prefix}" (at 0 {height/2 + 2.54} 0) (effects (font (size 1.27 1.27))))
      (property "Value" "{name}" (at 0 {-height/2 - 2.54} 0) (effects (font (size 1.27 1.27))))
      (property "LCSC" "{comp['lcsc']}" (at 0 {-height/2 - 5.08} 0) (effects (font (size 1.27 1.27)) hide))
      (symbol "{sym_name}_0_1"
        (rectangle (start -7.62 {height/2}) (end 7.62 {-height/2}) (stroke (width 0.254) (type default)) (fill (type background)))
      )
      (symbol "{sym_name}_1_1"'''
    
    # Add pins
    for i in range(1, pins + 1):
        y_pos = height/2 - 2.54 - (i-1) * (height - 2.54) / max(pins - 1, 1) if pins > 1 else 0
        if i <= pins // 2 or pins <= 2:
            symbol += f'''
        (pin passive line (at -10.16 {y_pos:.2f} 0) (length 2.54) (name "{i}" (effects (font (size 1.27 1.27)))) (number "{i}" (effects (font (size 1.27 1.27)))))'''
        else:
            symbol += f'''
        (pin passive line (at 10.16 {y_pos:.2f} 180) (length 2.54) (name "{i}" (effects (font (size 1.27 1.27)))) (number "{i}" (effects (font (size 1.27 1.27)))))'''
    
    symbol += '''
      )
    )'''
    return symbol

def generate_component_instance(ref, comp, x, y):
    """Generate a component instance"""
    name = comp["name"]
    sym_name = get_symbol_name(ref, comp)
    return f'''
  (symbol (lib_id "{sym_name}") (at {x} {y} 0) (unit 1)
    (in_bom yes) (on_board yes) (dnp no)
    (uuid {gen_uuid()})
    (property "Reference" "{ref}" (at {x} {y + 5.08} 0) (effects (font (size 1.27 1.27))))
    (property "Value" "{name}" (at {x} {y - 5.08} 0) (effects (font (size 1.27 1.27))))
    (property "LCSC" "{comp['lcsc']}" (at {x} {y - 7.62} 0) (effects (font (size 1.27 1.27)) hide))
  )'''

def generate_global_label(name, x, y, direction="input"):
    """Generate a global label"""
    shape = "input" if direction == "input" else "output" if direction == "output" else "bidirectional"
    return f'''
  (global_label "{name}" (shape {shape}) (at {x} {y} 0) (effects (font (size 1.27 1.27)) (justify left))
    (uuid {gen_uuid()})
  )'''

# Power net labels
POWER_NETS = ["+12V", "+3V3", "+5V", "GND", "VBUS"]

# Signal nets from netlist
SIGNAL_NETS = [
    "USB_DP", "USB_DM", "CC1", "CC2",
    "SW_NODE", "FB_NODE", "VOUT_PRE",
    "MCP_SCK", "MCP_MOSI", "MCP_MISO", "MCP_CS", "MCP_INT",
    "CANH", "CANL",
    "PWM_RAW", "PWM_FILTERED", "PWM_AMPLIFIED", "OPAMP_FB",
    "COMM_MOSI", "COMM_MISO", "COMM_SCK", "COMM_CS",
    "SD_CS", "SD_MOSI", "SD_SCK", "SD_MISO", "SD_CD",
    "JTAG_TCK", "JTAG_TDO", "JTAG_TDI", "JTAG_TMS",
    "UART_TX", "UART_RX",
    "EN", "IO0",
    "RPM_12V_IN", "RPM_LED_CATHODE", "RPM_LED_ANODE", "RPM_COLLECTOR",
    "VSS_VR_POS", "VSS_VR_NEG", "VSS_RSET", "VSS_OUTPUT",
    "I2C_SDA", "I2C_SCL", "MCP23017_INT",
    "ENC1_CLK", "ENC1_DT", "ENC1_SW",
    "ENC2_CLK", "ENC2_DT", "ENC2_SW",
    "ENC3_CLK", "ENC3_DT", "ENC3_SW",
    "ENC4_CLK", "ENC4_DT", "ENC4_SW",
    "ENC5_CLK", "ENC5_DT", "ENC5_SW",
    "WATER_TEMP_PULLUP", "WATER_TEMP_SENSE", "WATER_TEMP_FILTERED", "WATER_TEMP_IN",
]

def main():
    schematic = '''(kicad_sch (version 20231120) (generator "custom_generator")

  (uuid ''' + gen_uuid() + ''')

  (paper "A3")

  (lib_symbols'''
    
    # Generate lib symbols
    for ref, comp in COMPONENTS.items():
        schematic += generate_lib_symbol(ref, comp)
    
    schematic += '''
  )
'''
    
    # Place components in a grid
    x_start, y_start = 50, 50
    x_spacing, y_spacing = 40, 30
    col = 0
    row = 0
    max_cols = 8
    
    for ref, comp in COMPONENTS.items():
        x = x_start + col * x_spacing
        y = y_start + row * y_spacing
        schematic += generate_component_instance(ref, comp, x, y)
        col += 1
        if col >= max_cols:
            col = 0
            row += 1
    
    # Add power net labels
    label_y = 20
    for i, net in enumerate(POWER_NETS):
        schematic += generate_global_label(net, 20 + i * 20, label_y, "bidirectional")
    
    # Add signal net labels
    label_y = 10
    for i, net in enumerate(SIGNAL_NETS):
        schematic += generate_global_label(net, 20 + (i % 10) * 25, label_y - (i // 10) * 5, "bidirectional")
    
    schematic += '''

  (sheet_instances
    (path "/" (page "1"))
  )
)
'''
    
    return schematic

if __name__ == "__main__":
    output = main()
    with open("/home/trashguy/Projects/esp32-car-control/hardware/master-pcb-easyeda/kicad/ESP32-S3-Master-PCB.kicad_sch", "w") as f:
        f.write(output)
    print("Generated ESP32-S3-Master-PCB.kicad_sch successfully!")
    print(f"File size: {len(output)} bytes")
