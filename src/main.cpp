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
#include "USB_FT232H_MPSSE.h"


#define LC_SEQ          0x70 // 4 bytes (unsigned long)
#define EEPROM_CONFIG_SIZE 0x80 // 更新対象の全サイズ

// --- 設定 ---
#define VERSION "0.5.24" // バージョン番号

bool os_booted = false;   // OS起動完了フラグ

unsigned int localPort = 8888;
EthernetUDP Udp;
char packetBuffer[1460];
std::string luaBuffer = "";

// ============================================================
// UECS標準インターバル・スケジューラ（非ブロッキング）
//   1sec / 10sec / 1min の3種類
//   永続Lua VM(g_lua_main)上の exec1sec/exec10sec/exec1min を
//   それぞれの周期で呼び出す
// ============================================================
#define INTERVAL_1SEC_MS   1000UL
#define INTERVAL_10SEC_MS  10000UL
#define INTERVAL_1MIN_MS   60000UL

static uint32_t last_1sec_ms  = 0;
static uint32_t last_10sec_ms = 0;
static uint32_t last_1min_ms  = 0;

// 起動時の初期化処理で呼ばれるLua program
#define STARTUP_LUA_FILE "startup.lua"
// 永続VMが読み込むスケジューラ用Luaファイル
#define SCHEDULER_LUA_FILE "scheduler.lua"

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
    // USBホストをOS起動時に確実に1度だけ初期化する
    myusb.begin();

    Serial.println("--- System Ready ---");
    os_booted = true;

    // 6. startup.lua の実行（対話実行用の使い捨てVMで一度だけ流す）
    if (SD.exists(STARTUP_LUA_FILE)) {
        execute_lua_file(STARTUP_LUA_FILE);
    } else {
        Serial.println("No startup found.");
    }

    // 7. スケジューラ専用の永続Lua VMを初期化
    //    exec1sec/exec10sec/exec1min の関数定義とグローバル変数初期化を
    //    scheduler.lua から読み込む
    init_persistent_lua(SCHEDULER_LUA_FILE);

    // インターバル基準時刻を起動時点に合わせる
    uint32_t now = millis();
    last_1sec_ms  = now;
    last_10sec_ms = now;
    last_1min_ms  = now;
}

// ============================================================
// Teensy標準の yield() をオーバーライド（OSの完全な非ブロッキング化）
// ============================================================
void yield() {
    #define SCB_ICSR_VECTACTIVE 0x000001FF
    // 【最重要の防御壁】
    // ARM Cortex-M の割り込みステータスを直接確認し、
    // 割り込み処理(ISR)の中から呼ばれた場合は即座にリターンする（クラッシュ防止）
    if (SCB_ICSR & SCB_ICSR_VECTACTIVE) return;

    // 再帰呼び出し防止
    static bool in_yield = false;
    if (in_yield) return;
    in_yield = true;

    if (os_booted) {
        // USBコアのタスク処理
        myusb.Task();
    }
    in_yield = false;
}

void loop() {
    // 1. バックグラウンドタスク（USB, LCD, ネットワーク等）の実行
    yield();
    // 重い処理は 10ms(100Hz) に1回に間引く（I2Cフリーズ・暴走防止）
    static uint32_t last_heavy_task_ms = 0;
    uint32_t now = millis();
    if (now - last_heavy_task_ms >= 10) {
        last_heavy_task_ms = now;
        update_os_display();
        execute_uecs_transmission();
        process_network_manager();
        process_incoming_uecs();
        check_uecs_lifespan();
        for (int i = 0; i < MAX_FT232H_DEVICES; i++) {
            if (ft_devices[i]) ft_devices[i]->processTimers();
        }
    }

    // ============================================================
    // UECS標準インターバル・スケジューラの起動判定（非ブロッキング）
    //   3つとも独立して判定するため、10secと1minが同時期限でも
    //   両方とも取りこぼさず実行される
    // ============================================================
    now = millis();

    if (now - last_1sec_ms >= INTERVAL_1SEC_MS) {
        last_1sec_ms = now;
        call_scheduled_function("exec1sec");
    }
    if (now - last_10sec_ms >= INTERVAL_10SEC_MS) {
        last_10sec_ms = now;
        call_scheduled_function("exec10sec");
    }
    if (now - last_1min_ms >= INTERVAL_1MIN_MS) {
        last_1min_ms = now;
        call_scheduled_function("exec1min");
    }

    // 2. UDPコマンドの待機と実行 (Executer機能／対話実行・使い捨てVM)
    int packetSize = Udp.parsePacket();
    if (packetSize) {
        memset(packetBuffer, 0, sizeof(packetBuffer));
        int len = Udp.read(packetBuffer, sizeof(packetBuffer) - 1);
        
        if (len > 0) {
            packetBuffer[len] = '\0';
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
                if (dotPos != std::string::npos) { 
                    luaBuffer += line.substr(0, dotPos);

                    Serial.print("Executing Lua: [");
                    Serial.print(luaBuffer.c_str());
                    Serial.println("]");

                    lua_State *L = luaL_newstate();
                    luaL_openlibs(L);
                    register_lua_functions(L);
                    lua_sethook(L, lua_os_hook, LUA_MASKCOUNT, 10000);
                    IPAddress ip = Ethernet.localIP();
                    String ipStr = String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
                    lua_pushstring(L, ipStr.c_str());
                    lua_setglobal(L, "my_ip");

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
