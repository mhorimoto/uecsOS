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

// Lua API: uecs.publish(type_str, ccm_str, room, region, order, priority, value)
int l_uecs_publish(lua_State *L) {
    const char* p_type_str = luaL_checkstring(L, 1); // "cnd", "ord" 等
    const char* p_ccm_str  = luaL_checkstring(L, 2); // "InAirTemp" 等
    
    // 文字列から type 識別文字を決定（例: cnd -> 'c', ord -> 'o'）
    char type_code = p_type_str[0]; 

    // 既存スロットの検索と更新
    for (int i = 0; i < MAX_UECS_SLOTS; i++) {
        if (uecs_slots[i].active && 
            strcmp(uecs_slots[i].ccmtype, p_ccm_str) == 0 && 
            uecs_slots[i].room == (uint8_t)luaL_checkinteger(L, 3)) {
            
            uecs_slots[i].value = (float)luaL_checknumber(L, 7);
            return 0;
        }
    }

    // 新規スロットへの登録
    for (int i = 0; i < MAX_UECS_SLOTS; i++) {
        if (!uecs_slots[i].active) {
            uecs_slots[i].type = type_code;
            // 固定長配列への安全なコピー 
            strncpy(uecs_slots[i].ccmtype, p_ccm_str, sizeof(uecs_slots[i].ccmtype) - 1);
            uecs_slots[i].ccmtype[sizeof(uecs_slots[i].ccmtype) - 1] = '\0';
            
            uecs_slots[i].room     = (uint8_t)luaL_checkinteger(L, 3);
            uecs_slots[i].region   = (uint8_t)luaL_checkinteger(L, 4); 
            uecs_slots[i].order    = (uint16_t)luaL_checkinteger(L, 5);
            uecs_slots[i].priority = (uint8_t)luaL_checkinteger(L, 6);
            uecs_slots[i].value    = (float)luaL_checknumber(L, 7);
            uecs_slots[i].active   = true;
            break;
        }
    }
    return 0;
}

void execute_uecs_transmission() {
    static uint32_t last_sent = 0;
    if (millis() - last_sent < 1000) return; // 1秒周期
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

        char xml[350]; 
        // typeコードからタグ名（cnd, ord 等）を復元
        const char* tag = (uecs_slots[i].type == 'o') ? "ord" : "cnd";

        // TIMEタグを含まない軽量なXML生成 [cite: 28]
        snprintf(xml, sizeof(xml),
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<UECS ver=\"1.00-E10\">"
            "<%s type=\"%s\" room=\"%d\" region=\"%d\" order=\"%d\" priority=\"%d\">%.2f</%s>"
            "<IP>%d.%d.%d.%d</IP>"
            "</UECS>",
            tag, uecs_slots[i].ccmtype, 
            uecs_slots[i].room, uecs_slots[i].region, uecs_slots[i].order, 
            uecs_slots[i].priority, uecs_slots[i].value, tag,
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
