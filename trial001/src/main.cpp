#include <Arduino.h>
#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>
#include <string>
#include <stdarg.h>
#include <stdio.h>

extern "C" {
    #include "my_basic.h"
}

// Ethernet設定
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
unsigned int localPort = 8888;  // GUIからの送信ポート

EthernetUDP Udp;
struct mb_interpreter_t* mb = NULL;
char packetBuffer[UDP_TX_PACKET_MAX_SIZE]; // 受信バッファ

// MY-BASIC エラーハンドラ
void _on_error(struct mb_interpreter_t* s, mb_error_e e, const char* m, const char* f, int p, unsigned short row, unsigned short col, int a) {
    Serial.printf("BASIC Error: %s at Row %d, Col %d\n", m, row, col);
}

// 出力をシリアルに転送するハンドラ関数
int _print_handler(struct mb_interpreter_t* s, const char* fmt, ...) {
    char buf[128];
    va_list arg;
    va_start(arg, fmt);
    vsnprintf(buf, sizeof(buf), fmt, arg);
    va_end(arg);
    Serial.print(buf);
    return MB_FUNC_OK;
}

// BASICから LED(1) や LED(0) と呼び出した時に実行される関数
int _user_led_control(struct mb_interpreter_t* s, void** l) {
    int result = MB_FUNC_OK;
    int_t state = 0;
    // 引数を取り出す（mb_pop_int は内部で引数の存在チェックも行う）
    result = mb_pop_int(s, l, &state);
    if (result != MB_FUNC_OK) {
        return result;
    }
    // 実際のピン制御
    digitalWrite(13, state ? HIGH : LOW);
    Serial.printf("LED State changed to: %d\n", (int)state);
    return result;
}

// 受信したBASICコードを実行する関数
void execute_basic_code(const char* code) {
    if (mb) {
        mb_close(&mb); // 以前の状態をクリア
        mb = NULL;
    }
    if (mb_open(&mb) != MB_FUNC_OK) {
        Serial.println("Failed to open MY-BASIC instance.");
        return;
    }
    mb_set_printer(mb, _print_handler);
    mb_set_error_handler(mb, _on_error);

    // ここでOS層の自作関数（GET_UECS_TEMP等）を再登録する
    mb_register_func(mb, "LED", _user_led_control);
    // register_uecs_functions(mb); 

    Serial.println("--- Executing New Logic ---");
    if (mb_load_string(mb, code, true) == MB_FUNC_OK) {
        mb_run(mb, true);
    }
    Serial.println("--- Execution Finished ---");
}

void setup() {
    Serial.begin(115200);
    uint32_t startTime = millis();

    pinMode(13, OUTPUT); // LEDピンを出力に設定
    while (!Serial && (millis() - startTime < 5000));

    Serial.println("========================================");
    Serial.println("UECS-OS on Teensy 4.1 : System Starting");
    Serial.println("========================================");

    // MY-BASIC初期化
    mb_init();
    Serial.println("MY-BASIC Initialized.");

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
}

void loop() {
    int packetSize = Udp.parsePacket();
    if (packetSize) {
        int len = Udp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE - 1);
        if (len > 0) {
            packetBuffer[len] = '\0'; // 文字列終端
            Serial.println("Received new logic via UDP.");
            
            // 受信した文字列をインタプリタに渡して実行
            execute_basic_code(packetBuffer);
        }
    }

    // ここでOS層の常時処理（UECS受信等）を行う
    // handle_uecs_stack();
}