#include "network_manager.h"
#include "system_config.h"
#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>
#include <EEPROM.h>

#define NETWORK_MANAGER_VERSION "0.0.3"

EthernetUDP NodeScanUdp;
const unsigned int NODESCAN_PORT = 16529;

void init_network_manager() {
    // NODESCAN用のUDPポートの待機を開始
    NodeScanUdp.begin(NODESCAN_PORT);
    Serial.println("Network: NODESCAN engine started on port 16529.");
}

void process_network_manager() {
    int packetSize = NodeScanUdp.parsePacket();
    extern void reply_to_ccmscan(IPAddress remoteIP, uint16_t remotePort);
    if (packetSize) {
        char packetBuffer[256];
        int len = NodeScanUdp.read(packetBuffer, sizeof(packetBuffer) - 1);
        
        if (len > 0) {
            packetBuffer[len] = '\0';
            
            // パケット内に "NODESCAN" という文字列が含まれているか簡易判定
            if (strstr(packetBuffer, "NODESCAN") != NULL || strstr(packetBuffer, "nodescan") != NULL) {
                
                // 1. EEPROMから UECS ID を取得 (6バイトを16進数文字列へ)
                char uecs_id_str[13];
                uint8_t uecs_id[6];
                for(int i=0; i<6; i++) uecs_id[i] = EEPROM.read(ADDR_UECS_ID + i);
                snprintf(uecs_id_str, sizeof(uecs_id_str), "%02X%02X%02X%02X%02X%02X", 
                         uecs_id[0], uecs_id[1], uecs_id[2], uecs_id[3], uecs_id[4], uecs_id[5]);

                // 2. EEPROMから VENDER NAME を取得し、末尾の空白やNULLをトリム
                char vender_name[17];
                for(int i=0; i<16; i++) vender_name[i] = EEPROM.read(ADDR_VENDER_NAME + i);
                vender_name[16] = '\0';
                for(int i=15; i>=0; i--) { 
                    if(vender_name[i]==' ' || vender_name[i]=='\0') vender_name[i]='\0'; 
                    else break; 
                }

                // 3. EEPROMから NODE NAME を取得し、末尾をトリム
                char node_name[17];
                for(int i=0; i<16; i++) node_name[i] = EEPROM.read(ADDR_NODE_NAME + i);
                node_name[16] = '\0';
                for(int i=15; i>=0; i--) { 
                    if(node_name[i]==' ' || node_name[i]=='\0') node_name[i]='\0'; 
                    else break; 
                }

                // 4. IPとMACの取得
                uint8_t mac[6];
                Ethernet.MACAddress(mac);
                IPAddress ip = Ethernet.localIP();

                // 5. UECS実用規約に基づくXML応答の組み立て
                char xml[512];
                snprintf(xml, sizeof(xml),
                    "<?xml version=\"1.0\"?>"
                    "<UECS ver=\"1.00-E10\">"
                    "<NODE>"
                    "<NAME>%s</NAME>"
                    "<VENDER>%s</VENDER>"
                    "<UECSID>%s</UECSID>"
                    "<IP>%d.%d.%d.%d</IP>"
                    "<MAC>%02X%02X%02X%02X%02X%02X</MAC>"
                    "</NODE>"
                    "</UECS>",
                    node_name, vender_name, uecs_id_str,
                    ip[0], ip[1], ip[2], ip[3],
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

                // 6. 要求元（PCや他のノード）のIPとポートへ直接返信
                IPAddress remIP = NodeScanUdp.remoteIP();
                NodeScanUdp.beginPacket(remIP, NodeScanUdp.remotePort());
                NodeScanUdp.write(xml);
                NodeScanUdp.endPacket();
                
                Serial.printf("Network: Replied to NODESCAN from %d.%d.%d.%d\n", remIP[0], remIP[1], remIP[2], remIP[3]);
            } else if (strstr(packetBuffer, "CCMSCAN") != NULL || strstr(packetBuffer, "ccmscan") != NULL) {
                reply_to_ccmscan(NodeScanUdp.remoteIP(), NodeScanUdp.remotePort());
                Serial.println("Network: Received CCMSCAN request, but will be handled by UECS engine.");
        }
    }
}