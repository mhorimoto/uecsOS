#include "lua_functions.h"
#define LUA_REGISTRY_VERSION "0.0.4"

// 【追記1】lua_executor.cpp で定義した登録関数を外部参照
extern void register_lua_usb_topology(lua_State *L);

extern "C" {
    int luaopen_uecs(lua_State *L);
    int luaopen_lcd(lua_State *L);
    int luaopen_i2c(lua_State *L);
    int luaopen_usb(lua_State *L);
    int luaopen_sd(lua_State *L);
    int luaopen_eeprom(lua_State *L);
}

void register_lua_functions(lua_State *L) {
// ヘルパー関数: モジュールを登録してスタックをクリアする
    auto reg_mod = [&](const char* name, lua_CFunction f) {
        luaL_requiref(L, name, f, 1);
        lua_pop(L, 1);
    };

    // --- 1. モジュール（名前空間を持つ関数群）の登録 ---
    reg_mod("uecs", luaopen_uecs);
    reg_mod("lcd",  luaopen_lcd);
    reg_mod("i2c",  luaopen_i2c);
    reg_mod("usb",  luaopen_usb); // USBもモジュールとして登録
    reg_mod("sd",   luaopen_sd);  // SDもモジュールとして登録
    reg_mod("eeprom", luaopen_eeprom); // EEPROMもモジュールとして登録 VERSION 0.0.2で追加

    // --- 2. グローバル関数の登録 ---
    auto reg_glob = [&](const char* name, lua_CFunction f) {
        lua_pushcfunction(L, f);
        lua_setglobal(L, name);
    };
    // System
    reg_glob("print", l_my_print);
    reg_glob("reset", l_teensy_reset);
    reg_glob("digitalWrite", l_digitalWrite);
    reg_glob("digitalRead", l_digitalRead);
    reg_glob("delay", l_delay);
    reg_glob("luamem", l_system_luamem); // VERSION 0.0.3で追加

    // LCD/I2C
    reg_glob("lcd_init", l_lcd_init);
    reg_glob("lcd_print", l_lcd_print);
    reg_glob("lcd_clear", l_lcd_clear);
    reg_glob("lcd_setCursor", l_lcd_setCursor);
    reg_glob("i2c_begin", l_i2c_begin);
    reg_glob("i2c_read", l_i2c_read);
    reg_glob("i2c_write", l_i2c_write);

    // USB
    reg_glob("usb_begin", l_usb_begin);
    reg_glob("usb_write", l_usb_write);
    reg_glob("usb_read", l_usb_read);
    // 【追記2】大元の登録処理にトポロジ関数も相乗りさせる
    register_lua_usb_topology(L);
}