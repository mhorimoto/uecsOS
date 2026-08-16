#include "lua_functions.h"
#include <TimeLib.h>

// 【追加】ネットワーク系の関数を呼び出せるようにする
#include "network_manager.h"
#include "libuecs.h"

#define LUA_HW_SYSTEM_VERSION "0.0.4"

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

    int l_delay(lua_State *L) {
        uint32_t ms = (uint32_t)luaL_checkinteger(L, 1);
        uint32_t start = millis();
    
        while (millis() - start < ms) {
            // ここでOSのバックグラウンドタスク(UECS通信など)を処理
            // process_network_manager();
            // execute_uecs_transmission();
        
            // --- ブレイク信号(Ctrl+C または ESC)の監視を追加 ---
            if (Serial.available()) {
                char c = Serial.read();
                if (c == 0x03 || c == 0x1B) { // 0x03: Ctrl+C, 0x1B: ESC
                    // Lua VMに対して即座にエラーを発生させ、スクリプト実行を強制終了
                    return luaL_error(L, "Interrupted by User (Ctrl+C / ESC)");
                }
            }
            yield(); // Teensyの内部タスクに時間を譲る
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