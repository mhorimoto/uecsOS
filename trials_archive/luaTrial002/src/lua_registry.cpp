#include "lua_functions.h"

void register_lua_functions(lua_State *L) {
    auto reg = [&](const char* name, lua_CFunction f) {
        lua_pushcfunction(L, f);
        lua_setglobal(L, name);
    };

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