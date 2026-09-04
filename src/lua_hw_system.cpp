#include "lua_functions.h"
#include <TimeLib.h>

// 【追加】ネットワーク系の関数を呼び出せるようにする
#include "network_manager.h"
#include "libuecs.h"

#define LUA_HW_SYSTEM_VERSION "0.0.7"
extern void run_os_background_tasks();

// グローバルポインタの実体定義（デフォルトはSerial）
Print* active_lua_out = &Serial;

extern "C" {
    // SDカード対応のカスタムdofile
    int l_my_dofile(lua_State *L) {
        const char *filename = luaL_checkstring(L, 1);
        char path[64];
        if (filename[0] != '/') {
            snprintf(path, sizeof(path), "/%s", filename);
        } else {
            strncpy(path, filename, sizeof(path));
            path[sizeof(path) - 1] = '\0';
        }

        if (!SD.exists(path)) {
            return luaL_error(L, "cannot open %s: No such file on SD", filename);
        }

        File f = SD.open(path, FILE_READ);
        if (!f) {
            return luaL_error(L, "failed to open %s", filename);
        }

        String script = "";
        while (f.available()) {
            script += (char)f.read();
        }
        f.close();

        // 現在のVMインスタンス上で直接評価・実行する
        if (luaL_dostring(L, script.c_str()) != LUA_OK) {
            return lua_error(L); // スクリプト内の文法・実行時エラーをそのまま上位へ伝播
        }
        return 0;
    }
    int l_my_print(lua_State *L) {
        int nargs = lua_gettop(L);
        for (int i=1; i <= nargs; i++) {
            const char *s = lua_tostring(L, i);
            if (s) active_lua_out->print(s);
            if (i < nargs) active_lua_out->print("\t");
        }
        active_lua_out->println();
        return 0;
    }

    int l_teensy_reset(lua_State *L) {
        active_lua_out->println("Reset trigger calling...");
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
        
        // 【修正】自前の待機ループ内でOSタスクを明示的に回す
        while (millis() - start < ms) {
            yield();                    // USBの維持
            run_os_background_tasks();  // LCD・ネットワークの維持

            // --- ブレイク信号(Ctrl+C または ESC)の監視 ---
            if (Serial.available()) {
                char c = Serial.read();
                if (c == 0x03 || c == 0x1B) { 
                    return luaL_error(L, "Interrupted by User (Ctrl+C / ESC)");
                }
            }
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