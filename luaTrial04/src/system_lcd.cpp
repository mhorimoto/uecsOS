#include "system_lcd.h"
#include <TimeLib.h>

// 実体の定義（lua_hw_lcd.cppからこちらに引っ越し）
LiquidCrystal_I2C lcd(0x3F, 20, 4);

void init_system_lcd(const char* version_str) {
    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("uecsOS Starting...");
    delay(1000);
    lcd.setCursor(0, 0);
    lcd.print("uecsOS BLD:");
    lcd.print(version_str);
}

// main.cppから引っ越し
void update_os_display() {
    static time_t prevDisplay = 0;
    if (now() != prevDisplay) {
        prevDisplay = now();
        char timeStr[32];
        sprintf(timeStr, "%04d/%02d/%02d %02d:%02d:%02d", 
                year(), month(), day(), hour(), minute(), second());
        
        lcd.setCursor(0, 3);
        lcd.print(timeStr); 
    }
}