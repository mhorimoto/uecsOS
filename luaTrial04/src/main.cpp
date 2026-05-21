#include "system_config.h"
//#include <Arduino.h>
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
#define VERSION "0.4.07"

bool os_booted = false;   // OS起動完了フラグ
bool ntp_synced = false;  // 時刻同期状態フラグ

IPAddress ntpServer(133, 243, 238, 164); // ntp.nict.jp
const int NTP_PACKET_SIZE = 48;
byte ntpBuffer[NTP_PACKET_SIZE];

EthernetUDP Udp,NtpUdp;
unsigned int localPort = 8888;
unsigned long localNtpPort = 8889;
char packetBuffer[1460];
std::string luaBuffer = "";

// プログラム保持用のマップ
std::map<int, std::string> lua_program;


// Teensy内蔵RTCから時刻を取得する関数（TimeLib用）
time_t getTeensy3Time() {
    return Teensy3Clock.get();
}

// NTPサーバーにリクエストを送信
void sendNTPpacket(IPAddress& address) {
    memset(ntpBuffer, 0, NTP_PACKET_SIZE);
    ntpBuffer[0] = 0b11100011;   // LI, Version, Mode
    NtpUdp.beginPacket(address, 123);
    NtpUdp.write(ntpBuffer, NTP_PACKET_SIZE);
    NtpUdp.endPacket();
}

// NTP同期を試みる関数
bool syncWithNTP() {
    NtpUdp.begin(localNtpPort);
    sendNTPpacket(ntpServer);
    uint32_t beginWait = millis();
    while (millis() - beginWait < 5000) {
        if (NtpUdp.parsePacket()) {
            NtpUdp.read(ntpBuffer, NTP_PACKET_SIZE);
            unsigned long highWord = word(ntpBuffer[40], ntpBuffer[41]);
            unsigned long lowWord = word(ntpBuffer[42], ntpBuffer[43]);
            time_t epoch = (highWord << 16 | lowWord) - 2208988800UL + (9 * 3600); // JST
            setTime(epoch);
            Teensy3Clock.set(epoch);
            ntp_synced = true;
            NtpUdp.stop();
            return true;
        }
    }
    NtpUdp.stop();
    return false;
}

// --- Luaファイル実行部 ---
void execute_lua_file(const char* filename) {
    if (!SD.exists(filename)) return;
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    register_lua_functions(L);
    
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
    setSyncProvider(getTeensy3Time);

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

void handle_serial_input(String line) {
    line.trim();
    if (line.length() == 0) return;

    // 1. 行番号の判定
    int first_space = line.indexOf(' ');
    String first_word = (first_space > 0) ? line.substring(0, first_space) : line;
    
    bool is_num = true;
    for (unsigned int i = 0; i < first_word.length(); i++) {
        if (!isDigit(first_word[i])) { is_num = false; break; }
    }

    if (is_num) {
        // 行番号あり：プログラムの登録
        int line_num = first_word.toInt();
        if (first_space > 0) {
            lua_program[line_num] = line.substring(first_space + 1).c_str(); // 行番号をキー、コードを値として保存
        } else {
            lua_program.erase(line_num); // 行番号のみは行削除
        }
        return;
    }

    // 2. コマンド判定（RUN, LIST, NEW, etc...）
    String cmd = line;
    cmd.toUpperCase();

    if (cmd == "RUN") {
        std::string full_script = "";
        for (auto const& [num, code] : lua_program) {
            full_script += code;
            full_script += "\n";
        }
        // Luaステートを作成して実行
        lua_State *L = luaL_newstate();
        luaL_openlibs(L);
        register_lua_functions(L);
        if (luaL_dostring(L, full_script.c_str()) != LUA_OK) {
            Serial.println(lua_tostring(L, -1));
        }
        lua_close(L);
        Serial.println("Ok");
    } else if (cmd == "LIST") {
        for (auto const& [num, code] : lua_program) {
            Serial.printf("%d %s\n", num, code.c_str());
        }
    } else if (cmd == "NEW") {
        lua_program.clear();
        Serial.println("Ok");
    } else if (cmd == "DIR") {
        File dir = SD.open("/");
        if (!dir) {
            Serial.println("Failed to open SD root.");
        } else {
            while (true) {
                File entry = dir.openNextFile();
                if (!entry) break;
                Serial.print(entry.name());
                if (entry.isDirectory()) {
                    Serial.println("/");
                } else {
                    Serial.print("\t");
                    Serial.print(entry.size());
                    Serial.println(" bytes");
                }
                entry.close();
            }
            dir.close();
        }
        Serial.println("Ok");
    } else if (cmd.startsWith("SAVE ")) {
        String filename = line.substring(5);
        filename.trim();
        save_lua_program(filename.c_str());
    } else if (cmd.startsWith("LOAD ")) {
        String filename = line.substring(5);
        filename.trim();
        load_lua_program(filename.c_str());
    } else {
        // 即時実行モード
        lua_State *L = luaL_newstate();
        luaL_openlibs(L);
        register_lua_functions(L);
        if (luaL_dostring(L, line.c_str()) != LUA_OK) {
            Serial.println(lua_tostring(L, -1));
        }
        lua_close(L);
    }
}

void loop() {
    //extern void execute_uecs_transmission(); // libuecs.cppの関数を呼び出すための宣言
    //extern void process_network_manager();  // ネットワークマネージャーの定期処理
    //extern void process_incoming_uecs();
    //extern void check_uecs_lifespan();
    // 1. 1秒ごとにLCDとシリアルに表示
    static time_t prevDisplay = 0;
    if (now() != prevDisplay) {
        prevDisplay = now();
        
        char timeStr[32];
        sprintf(timeStr, "%04d/%02d/%02d %02d:%02d:%02d", 
                year(), month(), day(), hour(), minute(), second());
        lcd.setCursor(0, 3);
        lcd.print(timeStr); // LCD表示処理（既存の関数へ timeStr を渡す）
    }
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
    static String serialBuffer = "";
    if (Serial.available()>0) {
        char c = Serial.read();
        if (c == '\b' || c == 127) { 
            // バックスペース（ASCII 8）または DEL（ASCII 127）の処理
            if (serialBuffer.length() > 0) {
                // 1. バッファから最後の1文字を削除
                serialBuffer.remove(serialBuffer.length() - 1);
                // 2. 画面上の文字を消す（戻る -> 空白で上書き -> もう一度戻る）
                Serial.print("\b \b");
            }
        } else if (c == '\n' || c == '\r') {
            if (serialBuffer.length() > 0) {
                Serial.println();
                handle_serial_input(serialBuffer);
                serialBuffer = "";
            }
        } else if (c >= 32 && c <= 126) { 
            // 制御文字（矢印キーなど）を除外した，通常の印字可能文字のみを受け付ける
            serialBuffer += c;
            Serial.print(c); // Teensy側から文字をエコーバックする
        }
    }
}
