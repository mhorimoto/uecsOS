#include "system_sd.h"
#include <SD.h>
#include "system_config.h" // sync_config_from_sd 等を呼ぶため
#include "system_lcd.h"    // lcdインスタンスを使うため

bool init_system_sd() {
    if (!SD.begin(BUILTIN_SDCARD)) {
        Serial.println("SD Card failed!");
        lcd.setCursor(0, 1);
        lcd.print("SD Init Failed!   ");
        return false;
    } else {
        Serial.println("SD Card initialized.");
        lcd.setCursor(0, 1);
        lcd.print("SD Init Success!  ");
        
        // SDカードが正常なら、設定をEEPROMに同期する
        sync_config_from_sd(); 
        return true;
    }
}