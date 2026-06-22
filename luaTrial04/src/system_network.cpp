#include "system_network.h"
#include <NativeEthernet.h>
#include "system_config.h" // EEPROMからの設定読み込み用
#include "system_lcd.h"    // LCD表示用

// main.cppで定義されているUDPインスタンスとポート番号を参照
extern EthernetUDP Udp;
extern unsigned int localPort;

void init_system_network() {
    uint8_t current_mac[6];
    
    // EEPROMからMACアドレスを読み込む
    load_mac_address(current_mac); 
    
    if (is_dhcp_enabled()) {
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
        // Static IPの設定
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
}