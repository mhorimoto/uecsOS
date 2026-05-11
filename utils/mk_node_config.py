#!/usr/bin/env python3
import struct
import sys
import os

# --- EEPROM Map 定義 ---
ADDR_UECS_ID = 0x00
ADDR_MAC     = 0x06
ADDR_DHCP    = 0x0C
ADDR_IP      = 0x10
ADDR_VENDER  = 0x40
ADDR_NODE    = 0x50
ADDR_SEQ     = 0x70

def build_binary(filename, mac_bytes, node_name, vender_name, dhcp_flag, lc_seq):
    # M304N固定 UECS ID (10 10 0C 03 00 0E)
    uecs_id = bytes.fromhex("10100C03000E")
    
    img = bytearray([0xFF] * 128)
    img[ADDR_UECS_ID:ADDR_UECS_ID+6] = uecs_id
    img[ADDR_MAC:ADDR_MAC+6] = mac_bytes
    img[ADDR_DHCP] = dhcp_flag
    
    # DHCP有効時はIP関連を0にする
    if dhcp_flag == 0xFF:
        img[ADDR_IP:ADDR_IP+16] = bytes([0]*16)

    # 固定長16バイトでパッキング
    img[ADDR_VENDER:ADDR_VENDER+16] = vender_name.encode('ascii')[:16].ljust(16, b'\0')
    img[ADDR_NODE:ADDR_NODE+16] = node_name.encode('ascii')[:16].ljust(16, b'\0')
    
    # リトルエンディアンでシーケンス番号を格納
    struct.pack_into('<L', img, ADDR_SEQ, lc_seq)

    with open(filename, "wb") as f:
        f.write(img)
    print(f"Success: {filename} (MAC: {':'.join(f'{b:02X}' for b in mac_bytes)}, SEQ: {lc_seq})")

def main():
    if len(sys.argv) < 2:
        print("Usage:")
        print("  python mk_node_config.py 02:A2:73:10:FF:AA (Direct MAC)")
        print("  python mk_node_config.py node_config.py    (From File)")
        return

    arg = sys.argv[1]

    if arg.endswith(".py"):
        # 入力ファイルとして読み込む場合
        import importlib.util
        spec = importlib.util.spec_from_file_location("cfg", arg)
        cfg = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cfg)
        
        mac_bytes = bytes(int(x, 16) for x in cfg.MAC_ADDR.split(':'))
        build_binary("config.bin", mac_bytes, cfg.NODE_NAME, cfg.VENDER_NAME, cfg.DHCP_FLAG, cfg.LC_SEQ)
    
    else:
        # 直接MACアドレスを指定する場合
        try:
            mac_bytes = bytes(int(x, 16) for x in arg.split(':'))
            if len(mac_bytes) != 6: raise ValueError
            # デフォルト値で生成
            build_binary("config.bin", mac_bytes, "M304N-Manual", "Holly & Company", 0xFF, 1)
        except:
            print("Error: Invalid MAC address format. Use 02:A2:73:10:XX:XX")

if __name__ == "__main__":
    main()
