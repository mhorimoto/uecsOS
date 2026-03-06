#ifndef LUA_FUNCTIONS_H
#define LUA_FUNCTIONS_H

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <USBHost_t36.h>

extern "C" {
    #include "lua/lua.h"
    #include "lua/lualib.h"
    #include "lua/lauxlib.h"
}

// 共有インスタンスの外部参照宣言
extern LiquidCrystal_I2C lcd;
extern USBHost myusb;
extern USBSerial_BigBuffer userial;


// 関数登録用メイン関数
void register_lua_functions(lua_State *L);

// 各カテゴリのプロトタイプ宣言
// System系
int l_my_print(lua_State *L);
int l_teensy_reset(lua_State *L);
int l_digitalWrite(lua_State *L);
int l_digitalRead(lua_State *L);
int l_delay(lua_State *L);

// SD系
int l_sd_dir(lua_State *L);
int l_sd_append(lua_State *L);
int l_sd_read(lua_State *L);

// LCD/I2C系
int l_lcd_init(lua_State *L);
int l_lcd_print(lua_State *L);
int l_lcd_clear(lua_State *L);
int l_lcd_setCursor(lua_State *L);
int l_i2c_begin(lua_State *L);
int l_i2c_read(lua_State *L);
int l_i2c_write(lua_State *L);

// USB系
int l_usb_begin(lua_State *L);
int l_usb_write(lua_State *L);
int l_usb_read(lua_State *L);

#endif
