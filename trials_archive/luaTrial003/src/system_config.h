#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include <Arduino.h>
#include <IPAddress.h>
#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>

// --- EEPROM アドレスマップ (M304互換) ---
#define ADDR_UECS_ID      0x00 // 6 bytes
#define ADDR_MAC          0x06 // 6 bytes
#define ADDR_DHCP_FLAG    0x0C // 1 byte (0xFF: DHCP, 0x00: Static)
#define ADDR_FIXED_IP     0x10 // 4 bytes
#define ADDR_FIXED_MASK   0x14 // 4 bytes
#define ADDR_FIXED_GW     0x18 // 4 bytes
#define ADDR_FIXED_DNS    0x1C // 4 bytes
#define ADDR_VENDER_NAME  0x40 // 16 bytes
#define ADDR_NODE_NAME    0x50 // 16 bytes
#define ADDR_DBGMSG       0x60 // 16 bytes
#define ADDR_LC_SEQ       0x70 // 4 bytes (unsigned long)

#define EEPROM_CONFIG_SIZE 0x80 // 128 bytes

// --- 関数プロトタイプ ---
void sync_config_from_sd();
void load_mac_address(uint8_t* mac_out);
bool is_dhcp_enabled();
void load_static_ip_config(IPAddress& ip, IPAddress& subnet, IPAddress& gateway, IPAddress& dns);
void teensyMAC(uint8_t *mac);
#endif
