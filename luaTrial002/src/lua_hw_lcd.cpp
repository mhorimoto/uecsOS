#include "lua_functions.h"

LiquidCrystal_I2C lcd(0x3F, 20, 4);

int l_lcd_init(lua_State *L) {
    lcd.init();
    lcd.backlight();
    return 0;
}

int l_lcd_print(lua_State *L) {
    const char* str = luaL_checkstring(L, 1);
    lcd.print(str);
    return 0;
}

int l_lcd_clear(lua_State *L) {
    lcd.clear();
    return 0;
}

int l_lcd_setCursor(lua_State *L) {
    int col = (int)luaL_checkinteger(L, 1);
    int row = (int)luaL_checkinteger(L, 2);
    lcd.setCursor(col, row);
    return 0;
}

int l_i2c_begin(lua_State *L) {
    Wire.begin();
    return 0;
}

int l_i2c_read(lua_State *L) {
    uint8_t addr = (uint8_t)luaL_checkinteger(L, 1);
    uint8_t len = (uint8_t)luaL_checkinteger(L, 2);
    Wire.requestFrom(addr, len);
    lua_newtable(L);
    for (int i = 1; i <= len; i++) {
        if (Wire.available()) {
            lua_pushinteger(L, Wire.read());
            lua_rawseti(L, -2, i);
        }
    }
    return 1;
}

int l_i2c_write(lua_State *L) {
    uint8_t addr = (uint8_t)luaL_checkinteger(L, 1);
    uint8_t reg  = (uint8_t)luaL_checkinteger(L, 2);
    uint8_t val  = (uint8_t)luaL_checkinteger(L, 3);
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(val);
    lua_pushinteger(L, Wire.endTransmission());
    return 1;
}