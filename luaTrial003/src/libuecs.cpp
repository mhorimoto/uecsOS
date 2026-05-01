#include "lua_functions.h"
#include "libuecs.h"
#include <Arduino.h>
#include <TimeLib.h>
#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>

extern "C" {
    #include "lua.h"
    #include "lualib.h"
    #include "lauxlib.h"
}
extern EthernetUDP Udp;
extern IPAddress my_ip;
extern bool ntp_synced; // main.cppの変数を参照するために追加

// 固定長構造体の配列を静的に確保（メモリ断片化を回避） 
CCMData uecs_slots[MAX_UECS_SLOTS];


// OS内部からスロットを直接更新するための関数
// p_type: 'S' (Send), 'R' (Receive)
// p_ccmtype: "cnd", "InAirTemp" 等
bool set_uecs_slot_internal(char p_type, const char* p_ccmtype, uint8_t room, uint8_t region, uint16_t order, uint8_t priority, float value,uint8_t num_digit, uint8_t decimal_places) {
    
    // SLOT 0番は「cndパケット」専用として特別に扱う設計 
    int target_slot = -1;
    if (strcmp(p_ccmtype, "cnd") == 0) {
        target_slot = 0;
    } else {
        // 既存の同じ項目を探す
        for (int i = 1; i < MAX_UECS_SLOTS; i++) {
            if (uecs_slots[i].active && strcmp(uecs_slots[i].ccmtype, p_ccmtype) == 0 && uecs_slots[i].room == room) {
                target_slot = i;
                break;
            }
        }
        // 見つからなければ空きスロットを探す
        if (target_slot == -1) {
            for (int i = 1; i < MAX_UECS_SLOTS; i++) {
                if (!uecs_slots[i].active) {
                    target_slot = i;
                    break;
                }
            }
        }
    }

    if (target_slot != -1) {
        uecs_slots[target_slot].type = p_type; // 'S' などを格納 [cite: 30]
        strncpy(uecs_slots[target_slot].ccmtype, p_ccmtype, sizeof(uecs_slots[target_slot].ccmtype) - 1);
        uecs_slots[target_slot].ccmtype[sizeof(uecs_slots[target_slot].ccmtype) - 1] = '\0';
        uecs_slots[target_slot].room = room;
        uecs_slots[target_slot].region = region;
        uecs_slots[target_slot].order = order;
        uecs_slots[target_slot].priority = priority;
        uecs_slots[target_slot].value = value;
        uecs_slots[target_slot].num_digit = num_digit;
        uecs_slots[target_slot].decimal_places = decimal_places;
        uecs_slots[target_slot].active = true;
        return true;
    }
    return false;
}

// Lua API: uecs.publish(type_str, ccm_str, room, region, order, priority, value, num_digit, decimal_places)
// Lua APIは内部関数を呼び出す
int l_uecs_publish(lua_State *L) {
    const char* type_str = luaL_checkstring(L, 1); // "S"
    const char* ccmtype  = luaL_checkstring(L, 2); // "cnd", "InAirTemp"
    uint8_t room         = (uint8_t)luaL_checkinteger(L, 3);
    uint8_t region       = (uint8_t)luaL_checkinteger(L, 4);
    uint16_t order       = (uint16_t)luaL_checkinteger(L, 5);
    uint8_t priority     = (uint8_t)luaL_checkinteger(L, 6);
    float val            = (float)luaL_checknumber(L, 7);
    uint8_t num_digit    = (uint8_t)luaL_checkinteger(L, 8);
    uint8_t decimal_places = (uint8_t)luaL_checkinteger(L, 9);

    set_uecs_slot_internal(type_str[0], ccmtype, room, region, order, priority, val,0,0);
    return 0;
}

void execute_uecs_transmission() {
    char vbuf[32],*ptr_buf;
    char xml[513]; 
    static uint32_t last_sent = 0;
    if (millis() - last_sent < 1000) return; // 1秒周期
    ptr_buf = &vbuf[0];
    last_sent = millis();

    // ブロードキャストアドレスの動的計算
    IPAddress ip = Ethernet.localIP();
    IPAddress mask = Ethernet.subnetMask();
    IPAddress broadcastIP;
    for (int i = 0; i < 4; i++) {
        broadcastIP[i] = (ip[i] & mask[i]) | (~mask[i]);
    }

    for (int i = 0; i < MAX_UECS_SLOTS; i++) {
        if (!uecs_slots[i].active) continue;

        // TIMEタグを含まない軽量なXML生成
        if (uecs_slots[i].decimal_places == 0) {
            sprintf(vbuf,"%d",(int)uecs_slots[i].value);
        } else {
            sprintf(vbuf,"\"%%.%df\"",uecs_slots[i].decimal_places);
            sprintf(vbuf, vbuf, uecs_slots[i].value);
        }
        snprintf(xml, sizeof(xml),
            "<?xml version=\"1.0\"?>"
            "<UECS ver=\"1.00-E10\">"
            "<DATA type=\"%s\" room=\"%d\" region=\"%d\" order=\"%d\" priority=\"%d\">%s</DATA>"
            "<IP>%d.%d.%d.%d</IP>"
            "</UECS>",
            uecs_slots[i].ccmtype, 
            uecs_slots[i].room, uecs_slots[i].region, uecs_slots[i].order, 
            uecs_slots[i].priority, vbuf,
            ip[0], ip[1], ip[2], ip[3]);

        Udp.beginPacket(broadcastIP, 16520);
        Udp.write(xml);
        Udp.endPacket();
    }
}

// --- 1. 個別のAPI関数 ---
static int l_uecs_time(lua_State *L) {
    char buf[32];
    // ISO8601形式: YYYY-MM-DDTHH:MM:SS
    sprintf(buf, "%04d-%02d-%02dT%02d:%02d:%02d", 
            year(), month(), day(), hour(), minute(), second());
    lua_pushstring(L, buf);
    return 1;
}

static int l_uecs_uptime(lua_State *L) {
    lua_pushinteger(L, millis() / 1000);
    return 1;
}

static int l_uecs_is_synced(lua_State *L) {
    lua_pushboolean(L, ntp_synced);
    return 1;
}


// --- 2. Lua用関数登録リスト ---
static const struct luaL_Reg uecs_funcs[] = {
    {"time",   l_uecs_time},
    {"uptime", l_uecs_uptime},
    {"is_synced",l_uecs_is_synced},
    {"publish",   l_uecs_publish}, 
    {NULL, NULL}
};

// --- 3. モジュール登録関数（外部から呼ばれる） ---
int luaopen_uecs(lua_State *L) {
    luaL_newlib(L, uecs_funcs);
    return 1;
}
