#include "system_config.h"
#include <SD.h>
#include <EEPROM.h>
#define SYSTEM_CONFIG_C_VERSION "0.0.1"

// NXP CPUの内蔵レジスタからMACを読み出す
void teensyMAC(uint8_t *mac) {
    uint32_t m1 = HW_OCOTP_MAC1;
    uint32_t m2 = HW_OCOTP_MAC0;
    mac[0] = m1 >> 8;
    mac[1] = m1 >> 0;
    mac[2] = m2 >> 24;
    mac[3] = m2 >> 16;
    mac[4] = m2 >> 8;
    mac[5] = m2 >> 0;
}

void sync_config_from_sd() {
    const char* filename = "nodebase.cfg";
    if (!SD.exists(filename)) {
        Serial.println("Config: nodebase.cfg not found on SD.");
        return;
    }

    File f = SD.open(filename, FILE_READ);
    if (!f) return;

    // 1. シーケンス番号 (0x70) の比較による更新判定
    unsigned long sd_seq = 0;
    f.seek(ADDR_LC_SEQ);
    f.read((uint8_t*)&sd_seq, sizeof(sd_seq));

    unsigned long eep_seq = 0;
    EEPROM.get(ADDR_LC_SEQ, eep_seq);

    // 初回(0xFFFFFFFF) または SD側が新しい場合に更新
    if (eep_seq == 0xFFFFFFFF || sd_seq > eep_seq) {
        Serial.printf("Config: Updating EEPROM (SEQ: %lu -> %lu)\n", eep_seq, sd_seq);
        f.seek(0);
        for (int addr = 0; addr < EEPROM_CONFIG_SIZE; addr++) {
            if (f.available()) {
                uint8_t b = f.read();
                EEPROM.update(addr, b); // 変更がある場合のみ書き込み
            }
        }
        Serial.println("Config: EEPROM sync completed.");
    } else {
        Serial.printf("Config: EEPROM is up-to-date (SEQ: %lu).\n", eep_seq);
    }
    f.close();
}

void load_mac_address(uint8_t* mac_out) {
    // 常にNXP提供のハードウェアMACを取得
    teensyMAC(mac_out);

    // EEPROM内のMACアドレス領域をハードウェアの値で強制同期（整合性維持）
    for (int i = 0; i < 6; i++) {
        EEPROM.update(ADDR_MAC + i, mac_out[i]);
    }
    
    Serial.printf("Network: Using NXP Hardware MAC (UAA): %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac_out[0], mac_out[1], mac_out[2], mac_out[3], mac_out[4], mac_out[5]);
}

bool is_dhcp_enabled() {
    return EEPROM.read(ADDR_DHCP_FLAG) == 0xFF;
}

// EEPROMから4バイトを読み出してIPAddressオブジェクトを生成するヘルパー
void load_ip_from_eeprom(int addr, IPAddress& ip_obj) {
    uint8_t bytes[4];
    for (int i = 0; i < 4; i++) {
        bytes[i] = EEPROM.read(addr + i);
    }
    ip_obj = IPAddress(bytes[0], bytes[1], bytes[2], bytes[3]);
}

// 固定IP関連の設定を安全にロードする
void load_static_ip_config(IPAddress& ip, IPAddress& subnet, IPAddress& gateway, IPAddress& dns) {
    load_ip_from_eeprom(ADDR_FIXED_IP, ip);
    load_ip_from_eeprom(ADDR_FIXED_MASK, subnet);
    load_ip_from_eeprom(ADDR_FIXED_GW, gateway);
    load_ip_from_eeprom(ADDR_FIXED_DNS, dns);
}