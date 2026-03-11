#include "lua_functions.h"

extern int luaopen_uecs(lua_State *L);
extern int luaopen_lcd(lua_State *L);
extern int luaopen_i2c(lua_State *L);

void register_lua_functions(lua_State *L) {
    auto reg = [&](const char* name, lua_CFunction f) {
        lua_pushcfunction(L, f);
        lua_setglobal(L, name);
    };

    // uecs.time(), uecs.uptime()
    luaL_requiref(L, "uecs", luaopen_uecs, 1);
    lua_pop(L, 1);

    // lcd.print(), lcd.clear()
    luaL_requiref(L, "lcd", luaopen_lcd, 1);
    lua_pop(L, 1);

    // i2c.read(), i2c.write()
    luaL_requiref(L, "i2c", luaopen_i2c, 1);
    lua_pop(L, 1);
    
    // System
    reg("print", l_my_print);
    reg("reset", l_teensy_reset);
    reg("digitalWrite", l_digitalWrite);
    reg("digitalRead", l_digitalRead);
    reg("delay", l_delay);

    // SD
    reg("dir", l_sd_dir);
    reg("sd_read", l_sd_read);
    reg("sd_append", l_sd_append);

    // LCD/I2C
    reg("lcd_init", l_lcd_init);
    reg("lcd_print", l_lcd_print);
    reg("lcd_clear", l_lcd_clear);
    reg("lcd_setCursor", l_lcd_setCursor);
    reg("i2c_begin", l_i2c_begin);
    reg("i2c_read", l_i2c_read);
    reg("i2c_write", l_i2c_write);

    // USB
    reg("usb_begin", l_usb_begin);
    reg("usb_write", l_usb_write);
    reg("usb_read", l_usb_read);

}