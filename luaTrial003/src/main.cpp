#include <Arduino.h>
#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>
#include <TimeLib.h>

// --- 設定 ---
IPAddress ntpServer(133, 243, 238, 164); // ntp.nict.jp
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];
EthernetUDP Udp;

// Teensy内蔵RTCから時刻を取得する関数（TimeLib用）
time_t getTeensy3Time() {
    return Teensy3Clock.get();
}

// NTPサーバーにリクエストを送信
void sendNTPpacket(IPAddress& address) {
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    packetBuffer[0] = 0b11100011;   // LI, Version, Mode
    Udp.beginPacket(address, 123);
    Udp.write(packetBuffer, NTP_PACKET_SIZE);
    Udp.endPacket();
}

// NTP同期を試みる関数
bool syncWithNTP() {
    Udp.begin(8888);
    sendNTPpacket(ntpServer);
    
    uint32_t beginWait = millis();
    while (millis() - beginWait < 5000) { // 5秒待機
        if (Udp.parsePacket()) {
            Udp.read(packetBuffer, NTP_PACKET_SIZE);
            unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
            unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
            unsigned long secsSince1900 = highWord << 16 | lowWord;
            
            const unsigned long seventyYears = 2208988800UL;
            time_t epoch = secsSince1900 - seventyYears;
            
            // 日本標準時(JST)への変換 (+9時間)
            epoch += 9 * 3600;
            
            // システム時刻とRTCの両方を更新（特権操作）
            setTime(epoch);
            Teensy3Clock.set(epoch);
            return true;
        }
    }
    return false; // タイムアウト
}

void setup() {
    Serial.begin(115200);
    // LCD初期化処理（既存）
    
    // Ethernet開始（DHCP等，既存の処理）
    // Ethernet.begin(mac, ...);

    // 内部RTCを同期ソースに設定
    setSyncProvider(getTeensy3Time);

    Serial.println("時刻同期を開始します．．．");
    if (syncWithNTP()) {
        Serial.println("NTP同期に成功しました．．．");
    } else {
        Serial.println("NTP同期に失敗しました．RTC値で継続します．．．");
    }
}

void loop() {
    // 1秒ごとにLCDとシリアルに表示
    static time_t prevDisplay = 0;
    if (now() != prevDisplay) {
        prevDisplay = now();
        
        char timeStr[32];
        sprintf(timeStr, "%04d/%02d/%02d %02d:%02d:%02d", 
                year(), month(), day(), hour(), minute(), second());
        
        Serial.println(timeStr);
        // LCD表示処理（既存の関数へ timeStr を渡す）
    }
}
