#include <Arduino.h>
#include <string>
#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>
#include <stdio.h>
#include <SD.h>
#include <SPI.h>
#include "lua_functions.h"

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

/**
 * SDカード内のLuaファイルを読み込んで実行するExecuter
 * @param filename 実行したいファイル名（例: "task1.lua"）
 */
void execute_lua_file(const char* filename) {
    if (!SD.exists(filename)) {
        Serial.printf("Executer Error: File '%s' not found.\n", filename);
        return;
    }

    Serial.printf("--- Executing File: %s ---\n", filename);
    
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    register_lua_functions(L); // ハードウェア制御関数の登録

    // C++からIPアドレスなどのシステム変数を渡す
    IPAddress ip = Ethernet.localIP();
    String ipStr = String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
    lua_pushstring(L, ipStr.c_str());
    lua_setglobal(L, "my_ip");

    File f = SD.open(filename);
    if (f) {
        String script = "";
        while (f.available()) script += (char)f.read();
        f.close(); // 読み終えたら即座に閉じる

        if (luaL_dostring(L, script.c_str()) != LUA_OK) {
            Serial.printf("Lua Runtime Error in %s: %s\n", filename, lua_tostring(L, -1));
        }
    }
    
    lua_close(L);
    Serial.println("--- Execution Finished ---");
}


void setup() {
    Serial.begin(115200);
    uint32_t startTime = millis();
 
    pinMode(13, OUTPUT); // LEDピンを出力に設定
    while (!Serial && (millis() - startTime < 10000)); // シリアル接続を待つ（最大10秒）
    delay(1000); // 安定のため少し待つ

    Serial.println("========================================");
    Serial.println("uecsOS on Teensy 4.1 : System Starting");
    Serial.println("Version: 2.1.0 - Lua 5.5 Integration");
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
    // --- 起動時の自動実行セクション ---
    if (SD.exists("startup.lua")) {
        Serial.println("Found startup.lua. Executing...");
        
        lua_State *L = luaL_newstate();
        luaL_openlibs(L);
        register_lua_functions(L);

        // C++からIPアドレスをLua変数 "my_ip" として渡す
        IPAddress ip = Ethernet.localIP();
        String ipStr = String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
        lua_pushstring(L, ipStr.c_str());
        lua_setglobal(L, "my_ip");

        // SDカードから読み込んで実行
        File f = SD.open("startup.lua");
        if (f) {
            String script = "";
            while (f.available()) script += (char)f.read();
            f.close();
            if (luaL_dostring(L, script.c_str()) != LUA_OK) {
                Serial.printf("Startup Error: %s\n", lua_tostring(L, -1));
            }
        }
        lua_close(L); // 一旦クローズしてメモリを解放
    }
    Serial.println("--- System Ready: Waiting for UDP Commands ---");
}

void loop() {
    int packetSize = Udp.parsePacket();
    if (packetSize) {
        memset(packetBuffer, 0, sizeof(packetBuffer));
        int len = Udp.read(packetBuffer, sizeof(packetBuffer) - 1);
        
        if (len > 0) {
            packetBuffer[len] = '\0';
            std::string line = packetBuffer;
            if (line.substr(0, 4) == "run(") {
                size_t first = line.find('"');
                size_t last = line.find('"', first + 1);
                if (first != std::string::npos && last != std::string::npos) {
                    std::string targetFile = line.substr(first + 1, last - first - 1);
                    execute_lua_file(targetFile.c_str());
                    return; // 実行が終わったら loop の先頭に戻る
                }
            }            
            // 終端記号 "." が送られてきたら実行開始
            if (line == ".\n" || line == ".\r\n" || line == ".") {
                Serial.println("--- Executing Accumulated Lua Code ---");
                
                lua_State *L = luaL_newstate();
                luaL_openlibs(L);
                register_lua_functions(L);

                // UDPでも "my_ip" が使えるように再登録
                IPAddress ip = Ethernet.localIP();
                String ipStr = String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
                lua_pushstring(L, ipStr.c_str());
                lua_setglobal(L, "my_ip");

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