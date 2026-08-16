#include "system_time.h"
#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>

bool ntp_synced = false;

// NTP関連のプライベート変数（このファイル内だけで完結する）
static IPAddress ntpServer(133, 243, 238, 164); // ntp.nict.jp
static const int NTP_PACKET_SIZE = 48;
static byte ntpBuffer[NTP_PACKET_SIZE];
static EthernetUDP NtpUdp;
static unsigned long localNtpPort = 8889;

// Teensy内蔵RTCから時刻を取得する関数
time_t getTeensy3Time() {
    return Teensy3Clock.get();
}

// NTPサーバーにリクエストを送信（内部用関数）
static void sendNTPpacket(IPAddress& address) {
    memset(ntpBuffer, 0, NTP_PACKET_SIZE);
    ntpBuffer[0] = 0b11100011;   // LI, Version, Mode
    NtpUdp.beginPacket(address, 123);
    NtpUdp.write(ntpBuffer, NTP_PACKET_SIZE);
    NtpUdp.endPacket();
}

// NTP同期処理
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

// 時刻管理システムの初期化
void init_system_time() {
    // 内部RTC同期設定
    setSyncProvider(getTeensy3Time);
}