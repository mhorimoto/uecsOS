#ifndef SYSTEM_LCD_H
#define SYSTEM_LCD_H

#include <Arduino.h>
#include <LiquidCrystal_I2C.h> // ライブラリのインクルード

// OS全体で共有するLCDインスタンス
extern LiquidCrystal_I2C lcd;

// OS用API
void init_system_lcd(const char* version_str);
void update_os_display();

#endif