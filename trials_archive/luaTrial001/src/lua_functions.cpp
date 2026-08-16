#include "lua_functions.h"

LiquidCrystal_I2C lcd(0x3F, 20, 4);

// Luaから呼ばれるCの関数

// ソフトウェアリセットを実行する関数
void teensy_reset() {
  // ARM Cortex-M7 システムリセット
  SCB_AIRCR = 0x05FA0004;
}

int l_my_print(lua_State *L) {
    int nargs = lua_gettop(L); // 引数の数を取得
    for (int i=1; i <= nargs; i++) {
        const char *s = lua_tostring(L, i); // 引数を文字列として取得
        if (s) Serial.print(s);
        if (i < nargs) Serial.print("\t");
    }
    Serial.println(); // 改行
    return 0; // 戻り値の数
}

// Lua引数: (none) または (path)
// 戻り値: ファイル名のテーブル { "file1.txt", "dir1/", "data.csv" }
int l_sd_dir(lua_State *L) {
    const char* path = luaL_optstring(L, 1, "/");
    File dir = SD.open(path);
    
    lua_newtable(L); // 戻り値用のテーブルを作成
    int index = 1;

    if (!dir || !dir.isDirectory()) {
        return 1; // 空のテーブルを返す
    }

    while (true) {
        File entry = dir.openNextFile();
        if (!entry) break; // ファイルがなくなったら終了

        // 名前を取得し，ディレクトリなら末尾に / を付ける
        String name = String(entry.name());
        if (entry.isDirectory()) {
            name += "/";
        }

        // テーブルに挿入: table[index] = name
        lua_pushstring(L, name.c_str());
        lua_rawseti(L, -2, index++);
        
        entry.close();
    }
    dir.close();
    return 1;
}

// Lua引数: (filename, text)
// 例: sd_append("log.txt", "Temp: 25.4")
int l_sd_append(lua_State *L) {
    const char* filename = luaL_checkstring(L, 1);
    const char* text = luaL_checkstring(L, 2);

    File dataFile = SD.open(filename, FILE_WRITE);
    if (dataFile) {
        dataFile.println(text);
        dataFile.close();
        lua_pushboolean(L, true);
    } else {
        lua_pushboolean(L, false);
    }
    return 1; // 成功・失敗を返す
}

// Lua引数: (filename)
// 例: text = sd_read("config.lua")
int l_sd_read(lua_State *L) {
    const char* filename = luaL_checkstring(L, 1);
    File dataFile = SD.open(filename);
    if (dataFile) {
        String content = "";
        while (dataFile.available()) {
            content += (char)dataFile.read();
        }
        dataFile.close();
        lua_pushstring(L, content.c_str());
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// Lua引数: (none)
// 例: reset()
int l_teensy_reset(lua_State *L) {
    teensy_reset();
    return 0; // 戻り値なし
}

// Lua引数: (pin_number, state)
// 例: digitalWrite(13, 1)
int l_digitalWrite(lua_State *L) {
    int pin = (int)luaL_checkinteger(L, 1);   // 第1引数：ピン番号
    int state = (int)luaL_checkinteger(L, 2); // 第2引数：HIGH(1) or LOW(0)
    
    pinMode(pin, OUTPUT);
    digitalWrite(pin, state);
    
    return 0; // 戻り値なし
}

// Lua引数: (ms)
// 例: delay(500)
int l_delay(lua_State *L) {
    int ms = (int)luaL_checkinteger(L, 1);
    delay(ms);
    return 0;
}

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

// Lua引数: (address, register, value)
int l_i2c_write(lua_State *L) {
    uint8_t addr = (uint8_t)luaL_checkinteger(L, 1);
    uint8_t reg  = (uint8_t)luaL_checkinteger(L, 2);
    uint8_t val  = (uint8_t)luaL_checkinteger(L, 3);

    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(val);
    uint8_t error = Wire.endTransmission();

    lua_pushinteger(L, error); // 0なら成功
    return 1;
}
// Luaに関数をまとめて登録するヘルパー
void register_lua_functions(lua_State *L) {
    lua_pushcfunction(L, l_my_print);
    lua_setglobal(L, "print");
    lua_pushcfunction(L, l_digitalWrite);
    lua_setglobal(L, "digitalWrite");
    lua_pushcfunction(L, l_delay);
    lua_setglobal(L, "delay");
    lua_pushcfunction(L, l_teensy_reset);
    lua_setglobal(L, "reset");
    lua_pushcfunction(L, l_sd_append);
    lua_setglobal(L, "sd_append");
    lua_pushcfunction(L, l_sd_read);
    lua_setglobal(L, "sd_read");
    lua_pushcfunction(L, l_sd_dir);
    lua_setglobal(L, "dir");

    auto reg = [&](const char* name, lua_CFunction f) {
        lua_pushcfunction(L, f);
        lua_setglobal(L, name);
    };
    reg("i2c_begin", l_i2c_begin);
    reg("i2c_write", l_i2c_write);
    reg("i2c_read", l_i2c_read);
    reg("lcd_init", l_lcd_init);
    reg("lcd_print", l_lcd_print);
    reg("lcd_clear", l_lcd_clear);
    reg("lcd_setCursor", l_lcd_setCursor);
}

