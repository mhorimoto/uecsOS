#include "lua_functions.h"
#define LUA_REGISTRY_VERSION "0.0.1"

//extern int luaopen_uecs(lua_State *L);
//extern int luaopen_lcd(lua_State *L);
//extern int luaopen_i2c(lua_State *L);

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

    // SD
    //reg_glob("dir", l_sd_dir);
    //reg_glob("sd_read", l_sd_read);
    //reg_glob("sd_append", l_sd_append);

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

}