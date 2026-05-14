#include "lua_functions.h"
#include "libuecs.h"
#include <Arduino.h>
#include <TimeLib.h>
#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>

#define LIBUECS_VERSION "0.0.5"

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

            if (uecs_slots[i].decimal_places == 0) {
                snprintf(vbuf, sizeof(vbuf), "%d", (int)uecs_slots[i].value);
            } else {
                snprintf(fmt, sizeof(fmt), "%%.%df", uecs_slots[i].decimal_places);
                snprintf(vbuf, sizeof(vbuf), fmt, uecs_slots[i].value);
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

int l_uecs_publish(lua_State *L) {
    const char* type_str = luaL_checkstring(L, 1);
    const char* ccmtype = luaL_checkstring(L, 2);
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

int l_uecs_depublish(lua_State *L) {
    const char* ccmtype = luaL_checkstring(L, 1);
    uint8_t room        = (uint8_t)luaL_checkinteger(L, 2);
    uint8_t region      = (uint8_t)luaL_checkinteger(L, 3);
    
    if (strcmp(ccmtype, "cnd") == 0) {
        uecs_slots[0].active = false; return 0;
    }
    for (int i = 1; i < MAX_UECS_SLOTS; i++) {
        if (uecs_slots[i].active && uecs_slots[i].room == room && uecs_slots[i].region == region && strcmp(uecs_slots[i].ccmtype, ccmtype) == 0) {
            uecs_slots[i].active = false; break;
        }
    }
    return 0;
}

// --- Luaからスロットの値を読み出す関数 ---
int l_uecs_get(lua_State *L) {
    const char* ccmtype = luaL_checkstring(L, 1);
    uint8_t room        = (uint8_t)luaL_optinteger(L, 2, 1); // 省略時は1
    uint8_t region      = (uint8_t)luaL_optinteger(L, 3, 1); // 省略時は1

    for (int i = 0; i < MAX_UECS_SLOTS; i++) {
        if (uecs_slots[i].active && 
            uecs_slots[i].room == room && 
            uecs_slots[i].region == region && 
            strcmp(uecs_slots[i].ccmtype, ccmtype) == 0) {
            
            // 値（数値）と、データが寿命内かどうかのフラグ（真偽値）の2つを返す
            lua_pushnumber(L, uecs_slots[i].value);
            lua_pushboolean(L, uecs_slots[i].valid);
            return 2; 
        }
    }
    
    // スロットが見つからない場合
    lua_pushnil(L);
    lua_pushboolean(L, false);
    return 2;
}

static const struct luaL_Reg uecs_funcs[] = {
    {"time",      l_uecs_time},
    {"uptime",    l_uecs_uptime},
    {"publish",   l_uecs_publish},
    {"depublish", l_uecs_depublish},
    {"get",       l_uecs_get},
    {NULL, NULL}
};

int luaopen_uecs(lua_State *L) {
    luaL_newlib(L, uecs_funcs); return 1;
}
