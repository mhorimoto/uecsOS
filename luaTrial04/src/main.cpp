#include "system_config.h"
#include "system_time.h"
#include "serial_shell.h"
#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>
#include <TimeLib.h>
#include <SD.h>
#include <EEPROM.h>
#include <string>
#include <map>
#include "network_manager.h"
#include "lua_functions.h"
#include "libuecs.h"


#define LC_SEQ          0x70 // 4 bytes (unsigned long)
#define EEPROM_CONFIG_SIZE 0x80 // 更新対象の全サイズ

// --- 設定 ---
#define VERSION "0.5.01" // バージョン番号

bool os_booted = false;   // OS起動完了フラグ

unsigned int localPort = 8888;
EthernetUDP Udp;
char packetBuffer[1460];
std::string luaBuffer = "";

// プログラム保持用のマップ
std::map<int, std::string> lua_program;

// --- OSの状態（時計）をLCDに表示する関数 ---
void update_os_display() {
    static time_t prevDisplay = 0;
    // now() が前回表示した時刻と変わっていれば更新（1秒に1回だけ実行される）
    if (now() != prevDisplay) {
        prevDisplay = now();
        
        char timeStr[32];
        sprintf(timeStr, "%04d/%02d/%02d %02d:%02d:%02d", 
                year(), month(), day(), hour(), minute(), second());
        
        // LCDへの出力
        lcd.setCursor(0, 3);
        lcd.print(timeStr); 
    }
}
// --- 究極の要塞化：Lua強制フック関数 ---
void lua_os_hook(lua_State *L, lua_Debug *ar) {
    extern void execute_uecs_transmission();
    extern void process_network_manager();
    extern void process_incoming_uecs();
    extern void check_uecs_lifespan();
    
    update_os_display();  // LCD表示の更新を強制的に行う
    // --- 緊急停止（Halt）シグナルの監視 ---
    while (Serial.available() > 0) {
        char c = Serial.read();
        // ASCII 3 (Ctrl+C) または ASCII 27 (ESC) を検知したら強制終了
        if (c == 3 || c == 27) {
            Serial.println("\n[OS] Emergency Halt Triggered!");
            // Lua VMにエラーを投げ込み、実行スタックを強制的に破壊してC++へ戻る
            luaL_error(L, "Script halted by OS Interrupt"); 
        }
    }
    // ネットワークエンジンを強制駆動
    execute_uecs_transmission();
    process_network_manager();
    process_incoming_uecs();
    check_uecs_lifespan();
    
    // Teensy純正のバックグラウンド処理（Ethernetのハードウェアバッファ等）も回す
    yield(); 
}

// --- Luaファイル実行部 ---
void execute_lua_file(const char* filename) {
    if (!SD.exists(filename)) return;
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    register_lua_functions(L);
    lua_sethook(L, lua_os_hook, LUA_MASKCOUNT, 10000);

    IPAddress ip = Ethernet.localIP();
    String ipStr = String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
    lua_pushstring(L, ipStr.c_str());
    lua_setglobal(L, "my_ip");

    File f = SD.open(filename);
    if (f) {
        String script = "";
        while (f.available()) script += (char)f.read();
        f.close();
        luaL_dostring(L, script.c_str());
    }
    lua_close(L);
}

void setup() {
    uint8_t current_mac[6];
    //extern bool set_uecs_slot_internal(char,const char*,uint8_t, uint8_t,uint16_t,uint8_t,float,uint8_t,uint8_t,uint16_t);
    //extern void init_uecs_network();
    Serial.begin(115200);
    uint32_t startTime = millis();
    while (!Serial && (millis() - startTime < 5000));

    // 1. LCD初期化
    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("uecsOS Starting...");
    delay(1000);
    lcd.setCursor(0, 0);
    lcd.print("uecsOS BLD:" VERSION);
    Serial.println("uecsOS Starting ... BLD: " VERSION);
    // 2. SDカード初期化
    if (!SD.begin(BUILTIN_SDCARD)) {
        Serial.println("SD Card failed!");
        lcd.setCursor(0, 1);
        lcd.print("SD Init Failed!");
    } else {
        Serial.println("SD Card initialized.");
        lcd.setCursor(0, 1);
        lcd.print("SD Init Success!");
        sync_config_from_sd(); // SDカードからEEPROMへの設定同期
    }
    load_mac_address(current_mac); // EEPROMからMACアドレスを読み込む
    if (is_dhcp_enabled()) {
        // 3. Ethernet初期化 (DHCP)
        Serial.println("Attempting DHCP...");
        lcd.setCursor(0, 2);
        lcd.print("DHCP Requesting...");
    
        if (Ethernet.begin(current_mac) == 0) {
            Serial.println("Failed to configure Ethernet using DHCP");
            lcd.setCursor(0, 2);
            lcd.print("DHCP Failed!      ");
        } else {
            IPAddress ip = Ethernet.localIP();
            Serial.print("IP Address: ");
            Serial.println(ip);
            lcd.setCursor(0, 2);
            lcd.print("IP: ");
            lcd.print(ip);
            Udp.begin(localPort);
        }
    } else {
        // Ethernet初期化 (Static)
        IPAddress ip, subnet, gateway, dns;
        load_static_ip_config(ip, subnet, gateway, dns);
        Ethernet.begin(current_mac, ip, dns, gateway, subnet);
        Serial.print("Static IP: ");
        Serial.println(ip);
        lcd.setCursor(0, 2);
        lcd.print("IP: ");
        lcd.print(ip);
        Udp.begin(localPort);
    }

    // 4. 内部RTC同期設定
    init_system_time();

    // 5. NTP同期
    if (syncWithNTP()) {
        Serial.println("NTP synced.");
    }
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

void save_lua_program(const char* filename) {
    File f = SD.open(filename, FILE_WRITE);
    if (f) {
        f.truncate(); // 既存の内容を消去
        for (auto const& [num, code] : lua_program) {
            f.println(code.c_str()); // 保存時は行番号を除去して純粋なLuaファイルにする
        }
        f.close();
        Serial.println("Saved to SD.");
    } else {
        Serial.println("Save failed.");
    }
}

void load_lua_program(const char* filename) {
    File f = SD.open(filename, FILE_READ); // ファイルを読み込みモードで開く
    if (!f) {
        Serial.print("Failed to load: ");
        Serial.println(filename);
        return;
    }

    lua_program.clear(); // 既存のメモリ上のプログラムを消去
    int line_num = 10;   // 10番からスタート

    while (f.available()) {
        String l = f.readStringUntil('\n');
        // Windowsの改行コード(\r\n)対策: 末尾の \r を除去
        if (l.length() > 0 && l[l.length() - 1] == '\r') {
            l.remove(l.length() - 1);
        }
        
        // 自動で10刻みの行番号を付与してマップに登録
        lua_program[line_num] = l.c_str();
        line_num += 10;
    }
    f.close();
    Serial.println("Loaded.");
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
