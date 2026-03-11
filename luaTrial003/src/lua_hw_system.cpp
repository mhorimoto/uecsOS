#include "lua_functions.h"

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
    SCB_AIRCR = 0x05FA0004;
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