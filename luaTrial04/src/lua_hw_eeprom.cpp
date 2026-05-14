// lua_hw_eeprom.cpp
#include "lua_functions.h"
#include <EEPROM.h>

extern "C" {
    // 1バイト読み込み: eeprom.read(addr)
    int l_eeprom_read(lua_State *L) {
        int addr = luaL_checkinteger(L, 1);
        lua_pushinteger(L, EEPROM.read(addr));
        return 1;
    }

    // 1バイト書き込み: eeprom.write(addr, val)
    int l_eeprom_write(lua_State *L) {
        int addr = luaL_checkinteger(L, 1);
        int val = luaL_checkinteger(L, 2);
        EEPROM.write(addr, val);
        return 0;
    }

    // EEPROMのサイズ取得: eeprom.length()
    int l_eeprom_length(lua_State *L) {
        lua_pushinteger(L, EEPROM.length()); // Teensy 4.1 は 4284 バイト
        return 1;
    }

    static const struct luaL_Reg eeprom_funcs[] = {
        {"read",   l_eeprom_read},
        {"write",  l_eeprom_write},
        {"length", l_eeprom_length},
        {NULL, NULL}
    };

    int luaopen_eeprom(lua_State *L) {
        luaL_newlib(L, eeprom_funcs);
        return 1;
    }
}