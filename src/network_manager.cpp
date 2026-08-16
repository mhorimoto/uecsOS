#include "network_manager.h"
#include "system_config.h"
#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>
#include <EEPROM.h>

#define NETWORK_MANAGER_VERSION "0.0.4"

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
    extern void reply_to_nodescan(IPAddress remoteIP, uint16_t remotePort);
    if (packetSize) {
        char packetBuffer[256];
        int len = NodeScanUdp.read(packetBuffer, sizeof(packetBuffer) - 1);
        
        if (len > 0) {
            packetBuffer[len] = '\0';
            
            // パケット内に "NODESCAN" という文字列が含まれているか簡易判定
            if (strstr(packetBuffer, "NODESCAN") != NULL || strstr(packetBuffer, "nodescan") != NULL) {
                reply_to_nodescan(NodeScanUdp.remoteIP(), NodeScanUdp.remotePort());
                IPAddress remIP = NodeScanUdp.remoteIP();
                Serial.printf("Network: Replied to NODESCAN from %d.%d.%d.%d\n", remIP[0], remIP[1], remIP[2], remIP[3]);
            } else if (strstr(packetBuffer, "CCMSCAN") != NULL || strstr(packetBuffer, "ccmscan") != NULL) {
                reply_to_ccmscan(NodeScanUdp.remoteIP(), NodeScanUdp.remotePort());
                Serial.println("Network: Received CCMSCAN request, but will be handled by UECS engine.");
            }
        }
    }
}