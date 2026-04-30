#include "lua_functions.h"
#include "libuecs.h"
#include <Arduino.h>
#include <TimeLib.h>
#include <NativeEthernetUdp.h>

extern "C" {
    #include "lua.h"
    #include "lualib.h"
    #include "lauxlib.h"
}
extern EthernetUDP Udp;
extern IPAddress my_ip;
extern bool ntp_synced; // main.cppの変数を参照するために追加

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

void send_uecs_cnd_packet(const CCMData& data) {
    char xml[256];
    char time_str[32];
    
    // UECS XML形式への整形（Lua側には見せない） 
    sprintf(xml, 
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<UECS ver=\"1.00-E10\">"
        "<DATA type=\"%s\" room=\"%d\" region=\"%d\" order=\"%d\" priority=\"%d\">%f</DATA>"
        "<IP>%d.%d.%d.%d</IP>"
        "</UECS>",
        data.type.c_str(), data.room, data.region, data.order, data.priority, data.value,
        my_ip[0], my_ip[1], my_ip[2], my_ip[3]);

    // ブロードキャスト送信
    IPAddress broadcastIP(my_ip[0], my_ip[1], my_ip[2], 255);
    Udp.beginPacket(broadcastIP, 16520); // UECS標準ポート
    Udp.write(xml);
    Udp.endPacket();
}

// --- 2. Lua用関数登録リスト ---
static const struct luaL_Reg uecs_funcs[] = {
    {"time",   l_uecs_time},
    {"uptime", l_uecs_uptime},
    {"is_synced",l_uecs_is_synced},
    {NULL, NULL}
};

// --- 3. モジュール登録関数（外部から呼ばれる） ---
int luaopen_uecs(lua_State *L) {
    luaL_newlib(L, uecs_funcs);
    return 1;
}
