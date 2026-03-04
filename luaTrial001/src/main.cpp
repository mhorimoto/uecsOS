#include <Arduino.h>

extern "C" {
    #include "lua/lua.h"
    #include "lua/lualib.h"
    #include "lua/lauxlib.h"
}

// Luaから呼ばれるCの関数
int l_my_print(lua_State *L) {
    int nargs = lua_gettop(L); // 引数の数を取得
    for (int i=1; i <= nargs; i++) {
        const char *s = lua_tostring(L, i); // 引数を文字列として取得
        if (s) Serial.print(s);
        if (i < nargs) Serial.print("\t");
    }
    Serial.println(); // 改行
    return 0; // 戻り値の数
}
// Lua引数: (pin_number, state)
// 例: digitalWrite(13, 1)
int l_digitalWrite(lua_State *L) {
    int pin = (int)luaL_checkinteger(L, 1);   // 第1引数：ピン番号
    int state = (int)luaL_checkinteger(L, 2); // 第2引数：HIGH(1) or LOW(0)
    
    pinMode(pin, OUTPUT);
    digitalWrite(pin, state);
    
    return 0; // 戻り値なし
}

// Lua引数: (ms)
// 例: delay(500)
int l_delay(lua_State *L) {
    int ms = (int)luaL_checkinteger(L, 1);
    delay(ms);
    return 0;
}

void setup() {
    Serial.begin(115200);
    while (!Serial); // シリアルモニタの接続を待機
    delay(1000);

    Serial.println("--- Lua on Teensy Start v 0.02 ---");

    // 1. Luaの状態（ステート）を初期化
    lua_State *L = luaL_newstate();
    // 2. 標準ライブラリ（print関数など）をロード
    luaL_openlibs(L);
    // Lua上の 'print' という名前を C++の l_my_print に紐付ける
    lua_pushcfunction(L, l_my_print);
    lua_setglobal(L, "print");

    lua_pushcfunction(L, l_digitalWrite);
    lua_setglobal(L, "digitalWrite");

    lua_pushcfunction(L, l_delay);
    lua_setglobal(L, "delay");

    // 3. 実行したいLuaスクリプト
    //const char* lua_script = "print('Hello from Lua 5.5 on Teensy!')";
    const char* lua_script = 
        "print('LED Blink Start from Lua...') "
        "for i=1, 5 do "
        "  digitalWrite(13, 1) "
        "  delay(200) "
        "  digitalWrite(13, 0) "
        "  delay(200) "
        "  print('Blink count: ' .. i) "
        "end "
        "print('LED Blink Finished!') ";
    // 4. スクリプトの実行
    if (luaL_dostring(L, lua_script) != LUA_OK) {
        // エラーがあればシリアルに出力
        Serial.println(lua_tostring(L, -1));
    }

    // 5. ステートのクローズ
    lua_close(L);
  
    Serial.println("--- Execution Finished ---");
}

void loop() {
    // 今回は1回実行するだけなので空
}
