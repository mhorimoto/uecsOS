#! /usr/bin/env python3
import struct
import sys

# --- EEPROM Map 定義 ---
ADDR_UECS_ID = 0x00
ADDR_MAC     = 0x06
ADDR_DHCP    = 0x0C
ADDR_IP      = 0x10
ADDR_MASK    = 0x14
ADDR_GW      = 0x18
ADDR_VENDER  = 0x40
ADDR_NODE    = 0x50
ADDR_SEQ     = 0x70

def decode_binary(filename):
    try:
        with open(filename, "rb") as f:
            img = f.read(128)
    except FileNotFoundError:
        print(f"Error: {filename} not found.")
        return

    # 1. UECS ID (6 bytes)
    uecs_id = img[ADDR_UECS_ID:ADDR_UECS_ID+6].hex().upper()
    
    # 2. MAC Address (6 bytes)
    mac_addr = ":".join(f"{b:02X}" for b in img[ADDR_MAC:ADDR_MAC+6])
    
    # 3. DHCP Flag (1 byte)
    dhcp_flag = img[ADDR_DHCP]
    
    # 4. IP Addresses (4 bytes each)
    def to_ip(data):
        return ".".join(map(str, data))

    ip_addr = to_ip(img[ADDR_IP:ADDR_IP+4])
    netmask = to_ip(img[ADDR_MASK:ADDR_MASK+4])
    defgw   = to_ip(img[ADDR_GW:ADDR_GW+4])
    
    # 5. Names (16 bytes, ASCII)
    # NULL文字や末尾の空白を削除
    vender_name = img[ADDR_VENDER:ADDR_VENDER+16].decode('ascii', errors='ignore').strip('\x00').strip()
    node_name   = img[ADDR_NODE:ADDR_NODE+16].decode('ascii', errors='ignore').strip('\x00').strip()
    
    # 6. LC_SEQ (unsigned long, 4 bytes, Little Endian)
    lc_seq = struct.unpack_into('<L', img, ADDR_SEQ)[0]

    # ソースコード形式で出力
    source_content = f"""# node_config.py (Generated from binary)
UECS_ID_HEX = "{uecs_id[:6]}"
UECS_ID_SUFFIX = "{uecs_id[8:]}"
MAC_ADDR = "{mac_addr}"
DHCP_FLAG = {hex(dhcp_flag)}
IP_ADDR = "{ip_addr}"
NETMASK = "{netmask}"
DEFGW   = "{defgw}"
VENDER_NAME = "{vender_name}"
NODE_NAME   = "{node_name}"
LC_SEQ = {lc_seq}
"""
    
    output_filename = "node_config_rev.py"
    with open(output_filename, "w", encoding="utf-8") as f:
        f.write(source_content)
    
    print(f"Success: Binary decoded and saved to {output_filename}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python config_to_source.py config.bin")
    else:
        decode_binary(sys.argv[1])
        
