#include "lua_functions.h"
#include <Arduino.h>
#include <TimeLib.h>
extern "C" {
    #include "lua.h"
    #include "lualib.h"
    #include "lauxlib.h"
}
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
