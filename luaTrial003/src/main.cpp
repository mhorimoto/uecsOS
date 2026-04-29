#include <Arduino.h>
#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>
#include <TimeLib.h>
#include <SD.h>
#include <string>
#include "lua_functions.h"


// --- 設定 ---
#define VERSION "0.3.5"

bool ntp_synced = false;  // 時刻同期状態フラグ
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ntpServer(133, 243, 238, 164); // ntp.nict.jp
const int NTP_PACKET_SIZE = 48;
byte ntpBuffer[NTP_PACKET_SIZE];

EthernetUDP Udp,NtpUdp;
unsigned int localPort = 8888;
unsigned long localNtpPort = 8889;
char packetBuffer[1460];
std::string luaBuffer = "";

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
    }

    // 3. Ethernet初期化 (DHCP)
    Serial.println("Attempting DHCP...");
    lcd.setCursor(0, 2);
    lcd.print("DHCP Requesting...");
    
    if (Ethernet.begin(mac) == 0) {
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

    // 4. 内部RTC同期設定
    setSyncProvider(getTeensy3Time);

    // 5. NTP同期
    if (syncWithNTP()) {
        Serial.println("NTP synced.");
    }

    // 6. startup.lua の実行
    if (SD.exists("startup.lua")) {
        execute_lua_file("startupA.lua");
    } else {
        Serial.println("No startupA.lua found.");
    }
    Serial.println("--- System Ready ---");
}

void loop() {
    // 1. 1秒ごとにLCDとシリアルに表示
    static time_t prevDisplay = 0;
    if (now() != prevDisplay) {
        prevDisplay = now();
        
        char timeStr[32];
        sprintf(timeStr, "%04d/%02d/%02d %02d:%02d:%02d", 
                year(), month(), day(), hour(), minute(), second());
        Serial.println(timeStr);
        lcd.setCursor(0, 3);
        lcd.print(timeStr); // LCD表示処理（既存の関数へ timeStr を渡す）
    }
    // 2. UDPコマンドの待機と実行 (Executer機能)
    int packetSize = Udp.parsePacket();
    if (packetSize) {
        Serial.printf("Raw Packet: %d bytes\n", packetSize);
        memset(packetBuffer, 0, sizeof(packetBuffer));
        int len = Udp.read(packetBuffer, sizeof(packetBuffer) - 1);
        
        if (len > 0) {
            packetBuffer[len] = '\0';
            std::string line = packetBuffer;

            // run("filename") コマンドの解析
            if (line.substr(0, 4) == "run(") {
                size_t first = line.find('"');
                size_t last = line.find('"', first + 1);
                if (first != std::string::npos && last != std::string::npos) {
                    execute_lua_file(line.substr(first + 1, last - first - 1).c_str());
                }
            }
            // 終端記号 "." で溜まったバッファを実行
            else if (line.find('.') != std::string::npos) { 
                // "." より前のコードをバッファに追加し、不要な空白や改行をトリミング
                std::string cmd = line.substr(0, line.find('.'));
                luaBuffer += cmd;

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
