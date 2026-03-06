#include <Arduino.h>
#include <string>
#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>
#include <stdio.h>
#include <SD.h>
#include <SPI.h>

extern "C" {
    #include "lua/lua.h"
    #include "lua/lualib.h"
    #include "lua/lauxlib.h"
}

#define MY_UDP_BUFFER_SIZE 1460

// Ethernet設定
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
unsigned int localPort = 8888;  // GUIからの送信ポート
EthernetUDP Udp;
char packetBuffer[MY_UDP_BUFFER_SIZE]; // 受信バッファ

std::string luaBuffer = ""; // 受信したコードを溜めるバッファ
bool processingCode = false;

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

void setup() {
    Serial.begin(115200);
    uint32_t startTime = millis();
    void register_lua_functions(lua_State *L);

// ... その他の既存コード ...
    pinMode(13, OUTPUT); // LEDピンを出力に設定
    while (!Serial && (millis() - startTime < 10000)); // シリアル接続を待つ（最大10秒）
    delay(1000); // 安定のため少し待つ

    Serial.println("========================================");
    Serial.println("uecsOS on Teensy 4.1 : System Starting");
    Serial.println("Version: 0.2.0 - Lua 5.5 Integration");
    Serial.println("Copyright (c) 2026 uecsOS Team");
    Serial.println("========================================");

    // Ethernetの初期化（DHCP開始）
    Serial.println("Attempting to get an IP address using DHCP...");
  
    // Ethernet.begin(mac) は成功すると 1、失敗すると 0 を返します
    if (Ethernet.begin(mac) == 0) {
        Serial.println("Failed to configure Ethernet using DHCP");
        // DHCPに失敗した場合の処理（静的IPにフォールバック、または停止）
        if (Ethernet.hardwareStatus() == EthernetNoHardware) {
            Serial.println("Ethernet shield was not found. Sorry, can't run without hardware. :(");
        } else if (Ethernet.linkStatus() == LinkOFF) {
            Serial.println("Ethernet cable is not connected.");
        }
    } else {
        // 無事にIPアドレスを取得できた場合
        Serial.print("DHCP Success! My IP address: ");
        Serial.println(Ethernet.localIP());
        Udp.begin(localPort); 
        Serial.printf("UDP Listening on port %d\n", localPort);
    }
    if (!SD.begin(BUILTIN_SDCARD)) {
        Serial.println("SD Card initialization failed!");
    } else {
        Serial.println("SD Card initialized.");
    }

    // 1. Luaの状態（ステート）を初期化
    lua_State *L = luaL_newstate();
    // 2. 標準ライブラリ（print関数など）をロード
    luaL_openlibs(L);
    register_lua_functions(L); // 共通関数で一括登録
    // 3. 実行したいLuaスクリプト
    //const char* lua_script = "print('Hello from Lua 5.5 on Teensy!')";
    const char* lua_script = 
        "print('LED Blink Start from Lua...') "
        "for i=1, 5 do "
        "  digitalWrite(13, 1) "
        "  delay(200) "
        "  digitalWrite(13, 0) "
        "  delay(200) "
        "  print('Blink count: ' .. i) "
        "end "
        "print('LED Blink Finished!') ";
    // 4. スクリプトの実行
    if (luaL_dostring(L, lua_script) != LUA_OK) {
        // エラーがあればシリアルに出力
        Serial.println(lua_tostring(L, -1));
    }

    // 5. ステートのクローズ
    lua_close(L);
  
    Serial.println("--- Execution Finished ---");
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
}

void loop() {
    int packetSize = Udp.parsePacket();
    if (packetSize) {
        memset(packetBuffer, 0, sizeof(packetBuffer));
        int len = Udp.read(packetBuffer, sizeof(packetBuffer) - 1);
        
        if (len > 0) {
            packetBuffer[len] = '\0';
            std::string line = packetBuffer;
            
            // 終端記号 "." が送られてきたら実行開始
            if (line == ".\n" || line == ".\r\n" || line == ".") {
                Serial.println("--- Executing Accumulated Lua Code ---");
                
                lua_State *L = luaL_newstate();
                luaL_openlibs(L);
                register_lua_functions(L);

                // 蓄積したバッファを実行
                if (luaL_dostring(L, luaBuffer.c_str()) != LUA_OK) {
                    Serial.print("Lua Error: ");
                    Serial.println(lua_tostring(L, -1));
                }

                lua_close(L);
                luaBuffer = ""; // バッファをクリア
                Serial.println("--- Execution Finished & Buffer Cleared ---");
            } else {
                // 終端でなければバッファに追記
                luaBuffer += line;
                luaBuffer += "\n"; // 改行を補完
                Serial.println("Line added to buffer...");
            }
        }
    }
}