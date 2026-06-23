#include "system_config.h"
#include "system_time.h"
#include "serial_shell.h"
#include "lua_executor.h"
#include "system_lcd.h"
#include "system_sd.h"
#include "system_network.h"
#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>
#include <TimeLib.h>
#include <string>
#include <map>
#include "network_manager.h"
#include "lua_functions.h"
#include "libuecs.h"


#define LC_SEQ          0x70 // 4 bytes (unsigned long)
#define EEPROM_CONFIG_SIZE 0x80 // 更新対象の全サイズ

// --- 設定 ---
#define VERSION "0.5.08" // バージョン番号

bool os_booted = false;   // OS起動完了フラグ

unsigned int localPort = 8888;
EthernetUDP Udp;
char packetBuffer[1460];
std::string luaBuffer = "";

void setup() {
    uint8_t current_mac[6];
    Serial.begin(115200);
    uint32_t startTime = millis();
    while (!Serial && (millis() - startTime < 5000));

    // 1. UI(LCD)初期化
    init_system_lcd(VERSION);
    Serial.println("uecsOS Starting ... BLD: " VERSION);
    // 2. ストレージ(SDカードとEEPROM設定)初期化
    init_system_sd();
    // 3. ネットワーク初期化
    init_system_network();
    // 4. 内部RTC同期設定
    init_system_time();
    if (syncWithNTP()) {
        Serial.println("NTP synced.");
    }
    // 5. UECSプロトコルスタックの初期化
    init_network_manager(); // 16529ポート開始 ネットワークマネージャーの初期化
    init_uecs_network();    // 16520ポート開始 UECSネットワークの初期化
    // OS側からSLOT 0番に cnd を初期登録 (type='S', ccmtype="cnd") [cite: 30]
    set_uecs_slot_internal('S', "cnd", 7, 1, 1, 29, 0,0,0,1);
    Serial.println("--- System Ready ---");
    os_booted = true;

    // 6. startup.lua の実行
    if (SD.exists("startup.lua")) {
        execute_lua_file("startupA.lua");
    } else {
        Serial.println("No startupA.lua found.");
    }
}

void loop() {
    // 1. 1秒ごとにLCDとシリアルに表示
    update_os_display();
    execute_uecs_transmission();
    process_network_manager();
    process_incoming_uecs();
    check_uecs_lifespan();
    // 2. UDPコマンドの待機と実行 (Executer機能)
    int packetSize = Udp.parsePacket();
    if (packetSize) {
        memset(packetBuffer, 0, sizeof(packetBuffer));
        int len = Udp.read(packetBuffer, sizeof(packetBuffer) - 1);
        
        if (len > 0) {
            packetBuffer[len] = '\0';
            // 【デバッグ追加】受信した生のバイト列を確認
            Serial.print("RAW UDP DATA: ");
            for(int i=0; i<len; i++) {
                Serial.printf("%02X ", (uint8_t)packetBuffer[i]);
            }
            Serial.println();
            std::string line = packetBuffer;

            // run("filename") コマンドの解析
            if (line.substr(0, 4) == "run(") {
                size_t first = line.find('"');
                size_t last = line.find('"', first + 1);
                if (first != std::string::npos && last != std::string::npos) {
                    execute_lua_file(line.substr(first + 1, last - first - 1).c_str());
                }
            } else {
                size_t dotPos = line.find_last_of('.');
                // 終端記号 "." で溜まったバッファを実行
                if (dotPos != std::string::npos) { 
                    // ドットより前の部分をすべてバッファに追加
                    luaBuffer += line.substr(0, dotPos);

                    // デバッグ出力：これから実行するコードを「"」で囲って表示
                    Serial.print("Executing Lua: [");
                    Serial.print(luaBuffer.c_str());
                    Serial.println("]");

                    lua_State *L = luaL_newstate();
                    luaL_openlibs(L);
                    register_lua_functions(L);
                    lua_sethook(L, lua_os_hook, LUA_MASKCOUNT, 10000);
                    // 環境変数のセットアップ
                    IPAddress ip = Ethernet.localIP();
                    String ipStr = String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
                    lua_pushstring(L, ipStr.c_str());
                    lua_setglobal(L, "my_ip");

                    // 実行とエラーハンドリング
                    if (luaL_dostring(L, luaBuffer.c_str()) != LUA_OK) {
                        Serial.print("Lua Runtime Error: ");
                        Serial.println(lua_tostring(L, -1));
                    }

                    lua_close(L);
                    luaBuffer = ""; 
                    Serial.println("--- Execution Finished ---");
                } else {
                    luaBuffer += line + "\n";
                }
            }
        }
    }
    process_serial_shell();
}
