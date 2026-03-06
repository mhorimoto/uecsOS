#ifndef LUA_FUNCTIONS_H
#define LUA_FUNCTIONS_H

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <LiquidCrystal_I2C.h>

extern LiquidCrystal_I2C lcd;

extern "C" {
    #include "lua/lua.h"
    #include "lua/lualib.h"
    #include "lua/lauxlib.h"
}

// 登録用の一括関数
void register_lua_functions(lua_State *L);

// 各関数の宣言（実体はcpp側）
int l_my_print(lua_State *L);
int l_sd_dir(lua_State *L);
int l_sd_append(lua_State *L);
int l_sd_read(lua_State *L);
int l_teensy_reset(lua_State *L);
int l_digitalWrite(lua_State *L);
int l_delay(lua_State *L);
int l_lcd_init(lua_State *L);
int l_lcd_print(lua_State *L);
int l_lcd_clear(lua_State *L);
int l_lcd_setCursor(lua_State *L);
int l_i2c_begin(lua_State *L);
int l_i2c_write(lua_State *L);
int l_i2c_read(lua_State *L);
#endif