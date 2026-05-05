#include "lua_functions.h"
#include <TimeLib.h>
#define LUA_HW_SYSTEM_VERSION "0.0.1"

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
}
// Lua側で uecs.time() として呼べる関数
static int l_get_uecs_time(lua_State *L) {
    char buf[32];
    sprintf(buf, "%04d-%02d-%02dT%02d:%02d:%02d", 
            year(), month(), day(), hour(), minute(), second());
    lua_pushstring(L, buf); // UECS形式の文字列を返す
    return 1;
}