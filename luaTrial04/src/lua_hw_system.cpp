#include "lua_functions.h"
#include <TimeLib.h>
#define LUA_HW_SYSTEM_VERSION "0.0.2"

extern "C" {
    int l_my_print(lua_State *L) {
        int nargs = lua_gettop(L);
        for (int i=1; i <= nargs; i++) {
            const char *s = lua_tostring(L, i);
            if (s) Serial.print(s);
            if (i < nargs) Serial.print("\t");
        }
        Serial.println();
        return 0;
    }

    int l_teensy_reset(lua_State *L) {
        Serial.println("Reset trigger calling...");
        delay(100);
        SCB_AIRCR = 0x05FA0004;
        *(volatile uint32_t *)0xE000ED0C = 0x05FA0004;
        return 0;
    }

    int l_digitalWrite(lua_State *L) {
        int pin = (int)luaL_checkinteger(L, 1);
        int state = (int)luaL_checkinteger(L, 2);
        pinMode(pin, OUTPUT);
        digitalWrite(pin, state);
        return 0;
    }

    // Lua引数: (pin, mode)
    // mode: 0 = INPUT (デフォルト), 1 = INPUT_PULLUP
    int l_digitalRead(lua_State *L) {
        int pin = (int)luaL_checkinteger(L, 1);
        int mode_flag = (int)luaL_optinteger(L, 2, 0); // 第2引数がなければ0

        if (mode_flag == 1) {
            pinMode(pin, INPUT_PULLUP);
        } else {
            pinMode(pin, INPUT);
        }
        int state = digitalRead(pin);
        lua_pushinteger(L, state);
        return 1; // 読み取った値を返す
    }

    int l_delay(lua_State *L) {
        int ms = (int)luaL_checkinteger(L, 1);
        delay(ms);
        return 0;
    }

    // Luaエンジンの現在のメモリ使用量（バイト）を取得する関数 VERSION 0.0.2で追加
    int l_system_luamem(lua_State *L) {
        // LUA_GCCOUNT はキロバイト単位，LUA_GCCOUNTB はその端数（バイト）を返します
        int kb = lua_gc(L, LUA_GCCOUNT, 0);
        int bytes = lua_gc(L, LUA_GCCOUNTB, 0);
        int total_bytes = (kb * 1024) + bytes;
        
        lua_pushinteger(L, total_bytes);
        return 1; // 戻り値の数
    }
}
