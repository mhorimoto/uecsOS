#include "lua_functions.h"
#include <TimeLib.h>

// 【追加】ネットワーク系の関数を呼び出せるようにする
#include "network_manager.h"
#include "libuecs.h"

#define LUA_HW_SYSTEM_VERSION "0.0.3"

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

    int l_digitalRead(lua_State *L) {
        int pin = (int)luaL_checkinteger(L, 1);
        int mode_flag = (int)luaL_optinteger(L, 2, 0); 

        if (mode_flag == 1) {
            pinMode(pin, INPUT_PULLUP);
        } else {
            pinMode(pin, INPUT);
        }
        int state = digitalRead(pin);
        lua_pushinteger(L, state);
        return 1; 
    }

    // 【大幅修正】Luaのdelayをスマート待機に変更
    int l_delay(lua_State *L) {
        int ms = (int)luaL_checkinteger(L, 1);
        uint32_t start = millis();
        
        // 指定されたミリ秒が経過するまで、ネットワークタスクを回し続ける
        while (millis() - start < (uint32_t)ms) {
            execute_uecs_transmission();
            process_network_manager();
            process_incoming_uecs();
            check_uecs_lifespan();
            
            // Teensy純正のバックグラウンド処理(Ethernet等)を動かすため1msだけ標準delay
            delay(1); 
        }
        return 0;
    }

    int l_system_luamem(lua_State *L) {
        int kb = lua_gc(L, LUA_GCCOUNT, 0);
        int bytes = lua_gc(L, LUA_GCCOUNTB, 0);
        int total_bytes = (kb * 1024) + bytes;
        lua_pushinteger(L, total_bytes);
        return 1; 
    }
}