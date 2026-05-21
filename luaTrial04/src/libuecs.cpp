#include "lua_functions.h"
#include "libuecs.h"
#include "system_config.h"
#include <Arduino.h>
#include <TimeLib.h>
#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>
#include <EEPROM.h>

#define LIBUECS_VERSION "0.0.9"

extern "C" {
    #include "lua.h"
    #include "lualib.h"
    #include "lauxlib.h"
}

extern IPAddress my_ip;

// UECS専用のUDPインスタンス (16520番ポート)
EthernetUDP UecsUdp;
const unsigned int UECS_PORT = 16520;

CCMData uecs_slots[MAX_UECS_SLOTS];

// UECSネットワーク初期化 (setupで呼ぶ)
void init_uecs_network() {
    UecsUdp.begin(UECS_PORT);
    Serial.println("UECS: CCM engine started on port 16520.");
}

// スロット登録関数
bool set_uecs_slot_internal(char p_type, const char* p_ccmtype, uint8_t room, uint8_t region, uint16_t order, uint8_t priority, float value, uint8_t num_digit, uint8_t decimal_places, uint16_t interval_sec) {
    int target_slot = -1;
    if (strcmp(p_ccmtype, "cnd") == 0) {
        target_slot = 0;
    } else {
        for (int i = 1; i < MAX_UECS_SLOTS; i++) {
            if (uecs_slots[i].active && uecs_slots[i].type == p_type && uecs_slots[i].room == room && uecs_slots[i].region == region && strcmp(uecs_slots[i].ccmtype, p_ccmtype) == 0) {
                target_slot = i; break;
            }
        }
        if (target_slot == -1) {
            for (int i = 1; i < MAX_UECS_SLOTS; i++) {
                if (!uecs_slots[i].active) { target_slot = i; break; }
            }
        }
    }
    
    if (target_slot != -1) {
        uecs_slots[target_slot].type = p_type; 
        strncpy(uecs_slots[target_slot].ccmtype, p_ccmtype, 20);
        uecs_slots[target_slot].ccmtype[20] = '\0';
        uecs_slots[target_slot].room = room;
        uecs_slots[target_slot].region = region;
        uecs_slots[target_slot].order = order;
        uecs_slots[target_slot].priority = priority;
        uecs_slots[target_slot].value = value;
        uecs_slots[target_slot].num_digit = num_digit;
        uecs_slots[target_slot].decimal_places = decimal_places;
        
        uecs_slots[target_slot].interval_sec = interval_sec;
        uecs_slots[target_slot].last_update_ms = 0; 
        uecs_slots[target_slot].valid = (p_type == 'S'); 
        uecs_slots[target_slot].active = true;
        return true;
    }
    return false;
}

// 送信エンジン
void execute_uecs_transmission() {
    char vbuf[64], fmt[32];
    char xml[512]; 
    uint32_t current_ms = millis();

    IPAddress ip = Ethernet.localIP();
    IPAddress mask = Ethernet.subnetMask();
    IPAddress broadcastIP;
    for (int i = 0; i < 4; i++) broadcastIP[i] = (ip[i] & mask[i]) | (~mask[i]);

    for (int i = 0; i < MAX_UECS_SLOTS; i++) {
        if (!uecs_slots[i].active || uecs_slots[i].type != 'S') continue;

        uint32_t interval_ms = uecs_slots[i].interval_sec * 1000;
        if (interval_ms == 0) interval_ms = 1000; 

        if (current_ms - uecs_slots[i].last_update_ms >= interval_ms) {
            uecs_slots[i].last_update_ms = current_ms;

            // 0番スロット(cnd)は float を無視して 32bit整数/HEX で出力する
            if (i == 0) {
                if (uecs_slots[i].cnd_is_hex) {
                    // 【修正】Warning対策: %lX と unsigned long キャストを使用
                    snprintf(vbuf, sizeof(vbuf), "0x%08lX", (unsigned long)uecs_slots[i].cnd_value);
                } else {
                    snprintf(vbuf, sizeof(vbuf), "%lu", (unsigned long)uecs_slots[i].cnd_value);
                }
            } else {
                // 通常のスロット (1〜9番) は今まで通り float で出力
                if (uecs_slots[i].decimal_places == 0) {
                    snprintf(vbuf, sizeof(vbuf), "%d", (int)uecs_slots[i].value);
                } else {
                    snprintf(fmt, sizeof(fmt), "%%.%df", uecs_slots[i].decimal_places);
                    snprintf(vbuf, sizeof(vbuf), fmt, uecs_slots[i].value);
                }
            }
            snprintf(xml, sizeof(xml),
                "<?xml version=\"1.0\"?>"
                "<UECS ver=\"1.00-E10\">"
                "<DATA type=\"%s\" room=\"%d\" region=\"%d\" order=\"%d\" priority=\"%d\">%s</DATA>"
                "<IP>%d.%d.%d.%d</IP>"
                "</UECS>",
                uecs_slots[i].ccmtype, uecs_slots[i].room, uecs_slots[i].region, 
                uecs_slots[i].order, uecs_slots[i].priority, vbuf,
                ip[0], ip[1], ip[2], ip[3]);

            // UecsUdp を使って送信
            UecsUdp.beginPacket(broadcastIP, UECS_PORT);
            UecsUdp.write(xml);
            UecsUdp.endPacket();
        }
    }
}

// 受信エンジン (高速パース)
void process_incoming_uecs() {
    int packetSize = UecsUdp.parsePacket();
    if (packetSize) {
        // 自分自身がブロードキャストしたパケット（ループバック）は無視する
        if (UecsUdp.remoteIP() == Ethernet.localIP()) return;

        char packetBuffer[512];
        int len = UecsUdp.read(packetBuffer, sizeof(packetBuffer) - 1);
        if (len > 0) {
            packetBuffer[len] = '\0';
            
            // <DATA タグがあるか確認
            const char* data_tag = strstr(packetBuffer, "<DATA ");
            if (!data_tag) return;

            char ccmtype[21] = {0};
            int room = 0, region = 0;
            float value = 0.0;

            // UECSでは type 属性の中身が CCMTYPE (例: type="InAirTemp")
            const char* p = strstr(data_tag, "type=\"");
            if (p) {
                p += 6;
                int i = 0;
                while (*p != '"' && *p != '\0' && i < 20) ccmtype[i++] = *p++;
                ccmtype[i] = '\0';
            }

            p = strstr(data_tag, "room=\"");
            if (p) room = atoi(p + 6);

            p = strstr(data_tag, "region=\"");
            if (p) region = atoi(p + 8);

            // 値の抽出 (タグの終端 '>' の次)
            const char* tag_end = strchr(data_tag, '>');
            if (tag_end) value = atof(tag_end + 1);

            // Rスロットと照合して更新
            for (int i = 0; i < MAX_UECS_SLOTS; i++) {
                if (!uecs_slots[i].active || uecs_slots[i].type != 'R') continue;
                if (uecs_slots[i].room == room && uecs_slots[i].region == region && strcmp(uecs_slots[i].ccmtype, ccmtype) == 0) {
                    
                    uecs_slots[i].value = value;
                    uecs_slots[i].last_update_ms = millis();
                    uecs_slots[i].valid = true;
                    
                    Serial.printf("UECS: [Recv] Slot %d (%s r:%d g:%d) = %.2f\n", i, ccmtype, room, region, value);
                    break; 
                }
            }
        }
    }
}

// 寿命監視エンジン
void check_uecs_lifespan() {
    uint32_t current_ms = millis();
    for (int i = 0; i < MAX_UECS_SLOTS; i++) {
        if (!uecs_slots[i].active || uecs_slots[i].type != 'R') continue;

        // 寿命 = 期待される送信周期 × 3
        uint32_t lifespan_ms = uecs_slots[i].interval_sec * 3 * 1000;
        
        if (uecs_slots[i].valid && (current_ms - uecs_slots[i].last_update_ms > lifespan_ms)) {
            uecs_slots[i].valid = false;
            Serial.printf("UECS: [Timeout] Slot %d (%s) data expired.\n", i, uecs_slots[i].ccmtype);
        }
    }
}

// --- Luaバインディング ---
static int l_uecs_time(lua_State *L) {
    char buf[32];
    sprintf(buf, "%04d-%02d-%02dT%02d:%02d:%02d", year(), month(), day(), hour(), minute(), second());
    lua_pushstring(L, buf); return 1;
}

static int l_uecs_uptime(lua_State *L) {
    lua_pushinteger(L, millis() / 1000); return 1;
}

// --- 【修正】関数内部にcnd保護を移動 ---
int l_uecs_publish(lua_State *L) {
    const char* type_str = luaL_checkstring(L, 1);
    const char* ccmtype = luaL_checkstring(L, 2);

    // cnd保護: publishで"cnd"を触ろうとしたら無視する
    if (strncmp(ccmtype, "cnd", 3) == 0) {
        Serial.println("UECS Warning: Use uecs.cnd() to update Slot 0. Normal publish ignored.");
        return 0; 
    }

    uint8_t room         = (uint8_t)luaL_checkinteger(L, 3);
    uint8_t region       = (uint8_t)luaL_checkinteger(L, 4);
    uint16_t order       = (uint16_t)luaL_checkinteger(L, 5);
    uint8_t priority     = (uint8_t)luaL_checkinteger(L, 6);
    float val            = (float)lua_tonumber(L, 7);
    uint8_t num_digit    = (uint8_t)luaL_checkinteger(L, 8);
    uint8_t decimal_places = (uint8_t)luaL_checkinteger(L, 9);
    uint16_t interval_sec = (uint16_t)luaL_optinteger(L, 10, 60);

    set_uecs_slot_internal(type_str[0], ccmtype, room, region, order, priority, val, num_digit, decimal_places, interval_sec);
    return 0;
}

// --- スロットの個別削除（パラメータ指定 or 番号指定） ---
int l_uecs_depublish(lua_State *L) {
    if (lua_type(L, 1) == LUA_TNUMBER) {
        int target_slot = (int)luaL_checkinteger(L, 1);
        if (target_slot == 0) {
            Serial.println("UECS: Cannot depublish Slot 0 (cnd).");
        } else if (target_slot >= 1 && target_slot < MAX_UECS_SLOTS) {
            uecs_slots[target_slot].active = false;
            uecs_slots[target_slot].valid = false;
            Serial.printf("UECS: Slot %d deactivated by index.\n", target_slot);
        }
        return 0;
    } else if (lua_type(L, 1) == LUA_TSTRING) {
        const char* ccmtype = luaL_checkstring(L, 1);
        uint8_t room        = (uint8_t)luaL_checkinteger(L, 2);
        uint8_t region      = (uint8_t)luaL_checkinteger(L, 3);
        uint16_t order      = (uint16_t)luaL_checkinteger(L, 4); 

        for (int i = 1; i < MAX_UECS_SLOTS; i++) {
            if (uecs_slots[i].active && 
                uecs_slots[i].room == room && 
                uecs_slots[i].region == region && 
                uecs_slots[i].order == order && 
                strcmp(uecs_slots[i].ccmtype, ccmtype) == 0) {
                
                uecs_slots[i].active = false;
                uecs_slots[i].valid = false;
                Serial.printf("UECS: Slot %d (%s) deactivated by parameters.\n", i, ccmtype);
                break; 
            }
        }
        return 0;
    }
    luaL_error(L, "Invalid arguments for uecs.depublish");
    return 0;
}

// --- 現在のスロット一覧をシリアルに出力する ---
int l_uecs_list(lua_State *L) {
    Serial.println("\n--- uecsOS Slot Monitor ---");
    Serial.println("Slot | T | CCMType              | Room | Reg | Ord | Pri | Interval | Status");
    Serial.println("------------------------------------------------------------------------------");
    
    for (int i = 0; i < MAX_UECS_SLOTS; i++) {
        if (uecs_slots[i].active) {
            char status[16];
            if (uecs_slots[i].type == 'S') {
                strcpy(status, "Active(S)");
            } else {
                strcpy(status, uecs_slots[i].valid ? "Valid(R)" : "Expired(R)");
            }
            
            Serial.printf("%4d | %c | %-20s | %4d | %3d | %3d | %3d | %8d | %s\n",
                          i,
                          uecs_slots[i].type,
                          uecs_slots[i].ccmtype,
                          uecs_slots[i].room,
                          uecs_slots[i].region,
                          uecs_slots[i].order,
                          uecs_slots[i].priority,
                          uecs_slots[i].interval_sec,
                          status);
        } else {
            Serial.printf("%4d | - | (Empty)              |   -  |  -  |  -  |  -  |        - | -\n", i);
        }
    }
    Serial.println("------------------------------------------------------------------------------\n");
    return 0;
}


// --- Luaからスロットの値を読み出す関数 ---
int l_uecs_get(lua_State *L) {
    const char* ccmtype = luaL_checkstring(L, 1);
    uint8_t room        = (uint8_t)luaL_optinteger(L, 2, 1); 
    uint8_t region      = (uint8_t)luaL_optinteger(L, 3, 1); 

    for (int i = 0; i < MAX_UECS_SLOTS; i++) {
        if (uecs_slots[i].active && 
            uecs_slots[i].room == room && 
            uecs_slots[i].region == region && 
            strcmp(uecs_slots[i].ccmtype, ccmtype) == 0) {
            
            lua_pushnumber(L, uecs_slots[i].value);
            lua_pushboolean(L, uecs_slots[i].valid);
            return 2; 
        }
    }
    lua_pushnil(L);
    return 2;
}

// --- スロットを一括解除する（0番のcndは保護） ---
void clear_uecs_slots() {
    for (int i = 1; i < MAX_UECS_SLOTS; i++) {
        uecs_slots[i].active = false;
        uecs_slots[i].valid = false;
    }
    Serial.println("UECS: All slots (except Slot 0) cleared.");
}

int l_uecs_clear(lua_State *L) {
    clear_uecs_slots();
    return 0;
}

// --- cndスロット(0番)専用の更新関数 ---
int l_uecs_cnd(lua_State *L) {
    const char* ccmtype = luaL_checkstring(L, 1);
    
    if (strlen(ccmtype) == 0 || strncmp(ccmtype, "cnd", 3) != 0) {
        luaL_error(L, "cnd ccmtype must start with 'cnd' and cannot be empty.");
        return 0;
    }

    uint8_t room     = (uint8_t)luaL_checkinteger(L, 2);
    uint8_t region   = (uint8_t)luaL_checkinteger(L, 3);
    uint16_t order   = (uint16_t)luaL_checkinteger(L, 4);
    uint8_t priority = (uint8_t)luaL_checkinteger(L, 5);

    uint32_t val = 0;
    bool is_hex = false;

    if (lua_type(L, 6) == LUA_TSTRING) {
        const char* val_str = lua_tostring(L, 6);
        if (strncmp(val_str, "0x", 2) == 0 || strncmp(val_str, "0X", 2) == 0) {
            val = strtoul(val_str, NULL, 16); 
            is_hex = true;
        } else {
            val = strtoul(val_str, NULL, 10); 
        }
    } else {
        val = (uint32_t)lua_tonumber(L, 6);   
    }

    uecs_slots[0].type = 'S';
    strncpy(uecs_slots[0].ccmtype, ccmtype, 20);
    uecs_slots[0].ccmtype[20] = '\0';
    uecs_slots[0].room = room;
    uecs_slots[0].region = region;
    uecs_slots[0].order = order;
    uecs_slots[0].priority = priority;
    
    uecs_slots[0].cnd_value = val;
    uecs_slots[0].cnd_is_hex = is_hex;
    
    uecs_slots[0].interval_sec = 1; 
    uecs_slots[0].active = true;
    uecs_slots[0].valid = true;

    return 0;
}

static const struct luaL_Reg uecs_funcs[] = {
    {"time",      l_uecs_time},
    {"uptime",    l_uecs_uptime},
    {"publish",   l_uecs_publish},
    {"depublish", l_uecs_depublish},
    {"list",      l_uecs_list},
    {"clear",     l_uecs_clear},
    {"get",       l_uecs_get},
    {"cnd",       l_uecs_cnd},
    {NULL, NULL}
};

// --- NODESCAN応答エンジン ---
void reply_to_nodescan(IPAddress remoteIP, uint16_t remotePort) {
    char xml[512];
    IPAddress ip = Ethernet.localIP();
    uint8_t mac[6];
    Ethernet.MACAddress(mac);
    char uecs_id_str[13];
    uint8_t uecs_id[6];
    char vender_name[17];
    char node_name[17];

    // 1. EEPROMから UECS ID を取得 (6バイトを16進数文字列へ)
    for(int i=0; i<6; i++) uecs_id[i] = EEPROM.read(ADDR_UECS_ID + i);
    snprintf(uecs_id_str, sizeof(uecs_id_str), "%02X%02X%02X%02X%02X%02X", 
                uecs_id[0], uecs_id[1], uecs_id[2], uecs_id[3], uecs_id[4], uecs_id[5]);

    // 2. EEPROMから VENDER NAME を取得し、末尾の空白やNULLをトリム
    for(int i=0; i<16; i++) vender_name[i] = EEPROM.read(ADDR_VENDER_NAME + i);
    vender_name[16] = '\0';
    for(int i=15; i>=0; i--) { 
        if(vender_name[i]==' ' || vender_name[i]=='\0') vender_name[i]='\0'; 
            else break; 
    }

    // 3. EEPROMから NODE NAME を取得し、末尾をトリム
    for(int i=0; i<16; i++) node_name[i] = EEPROM.read(ADDR_NODE_NAME + i);
    node_name[16] = '\0';
    for(int i=15; i>=0; i--) { 
        if(node_name[i]==' ' || node_name[i]=='\0') node_name[i]='\0'; 
            else break; 
    }

    // UECS規約に準拠したNODE情報XMLの組み立て
    snprintf(xml, sizeof(xml),
            "<?xml version=\"1.0\"?>"
            "<UECS ver=\"1.00-E10\">"
            "<NODE>"
            "<NAME>%s</NAME>"
            "<VENDER>%s</VENDER>"
            "<UECSID>%s</UECSID>"
            "<IP>%d.%d.%d.%d</IP>"
            "<MAC>%02X%02X%02X%02X%02X%02X</MAC>"
            "</NODE>"
            "</UECS>",
            node_name, vender_name, uecs_id_str,
            ip[0], ip[1], ip[2], ip[3],
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // UecsUdp（16520番のインスタンス）を使い回すか，
    // あるいは送信元のポート（16529など）へ確実に撃ち返す
    UecsUdp.beginPacket(remoteIP, remotePort);
    UecsUdp.write(xml);
    UecsUdp.endPacket();

    Serial.printf("UECS: Replied to NODESCAN from %d.%d.%d.%d\n", remoteIP[0], remoteIP[1], remoteIP[2], remoteIP[3]);
}
// --- CCMSCAN応答エンジン ---
void reply_to_ccmscan(IPAddress remoteIP, uint16_t remotePort) {
    char xml[1024]; // 全スロット(最大10)を収容するためのバッファ
    char ccm_buf[128];
    
    IPAddress ip = Ethernet.localIP();
    uint8_t mac[6];
    Ethernet.MACAddress(mac); // NativeEthernetの標準関数でMACを取得

    // UECS XMLヘッダとノード情報
    snprintf(xml, sizeof(xml),
        "<?xml version=\"1.0\"?>\n"
        "<UECS ver=\"1.00-E10\">\n"
        "<IP>%d.%d.%d.%d</IP>\n"
        "<MAC>%02X%02X%02X%02X%02X%02X</MAC>\n",
        ip[0], ip[1], ip[2], ip[3],
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // アクティブな全スロットをXMLタグ化して追加
    for (int i = 0; i < MAX_UECS_SLOTS; i++) {
        if (uecs_slots[i].active) {
            snprintf(ccm_buf, sizeof(ccm_buf),
                "<CCM type=\"%c\" room=\"%d\" region=\"%d\" order=\"%d\" priority=\"%d\" ccmtype=\"%s\" />\n",
                uecs_slots[i].type,
                uecs_slots[i].room,
                uecs_slots[i].region,
                uecs_slots[i].order,
                uecs_slots[i].priority,
                uecs_slots[i].ccmtype);
            
            // バッファオーバーフロー防止を兼ねて結合
            strncat(xml, ccm_buf, sizeof(xml) - strlen(xml) - 1);
        }
    }
    
    // フッタを追加
    strncat(xml, "</UECS>", sizeof(xml) - strlen(xml) - 1);

    // 要求元(PC等)のIPとポートへ返信
    UecsUdp.beginPacket(remoteIP, remotePort);
    UecsUdp.write(xml);
    UecsUdp.endPacket();
    
    Serial.printf("UECS: Replied to CCMSCAN from %d.%d.%d.%d\n", remoteIP[0], remoteIP[1], remoteIP[2], remoteIP[3]);
}

int luaopen_uecs(lua_State *L) {
    luaL_newlib(L, uecs_funcs); return 1;
}