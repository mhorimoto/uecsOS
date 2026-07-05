#include "lua_executor.h"
#include <SD.h>
#include <NativeEthernet.h>
#include "USB_FT232H_MPSSE.h"

// main.cpp に残るUI関数とネットワークエンジンの外部参照
extern void update_os_display(); 
extern void execute_uecs_transmission();
extern void process_network_manager();
extern void process_incoming_uecs();
extern void check_uecs_lifespan();

// lua_hw_usb.cpp で定義されているFT232Hインスタンスの外部参照
// processTimers() をフック内で呼ぶことで、Luaが無限ループに陥っても
// タイマーによるリレーOFF処理が継続して動作する
extern FT232H_MPSSE ft232h;

// プログラム保持用のマップの実体
std::map<int, std::string> lua_program;

// --- 究極の要塞化：Lua強制フック関数 ---
void lua_os_hook(lua_State *L, lua_Debug *ar) {
    update_os_display();  // LCD表示の更新を強制的に行う
    
    // --- 緊急停止（Halt）シグナルの監視 ---
    while (Serial.available() > 0) {
        char c = Serial.read();
        // ASCII 3 (Ctrl+C) または ASCII 27 (ESC) を検知したら強制終了
        if (c == 3 || c == 27) {
            Serial.println("\n[OS] Emergency Halt Triggered!");
            luaL_error(L, "Script halted by OS Interrupt"); 
        }
    }
    
    // ネットワークエンジンを強制駆動
    execute_uecs_transmission();
    process_network_manager();
    process_incoming_uecs();
    check_uecs_lifespan();

    // リレー・タイマー処理を強制駆動
    // Luaが無限ループ中でも 10000命令ごとに呼ばれるため、
    // ft_pin_pulse() のOFF処理がブロックされない
    ft232h.processTimers();
    
    // Teensy純正のバックグラウンド処理
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

void save_lua_program(const char* filename) {
    File f = SD.open(filename, FILE_WRITE);
    if (f) {
        f.truncate(); // 既存の内容を消去
        for (auto const& [num, code] : lua_program) {
            f.println(code.c_str()); // 保存時は行番号を除去
        }
        f.close();
        Serial.println("Saved to SD.");
    } else {
        Serial.println("Save failed.");
    }
}

void load_lua_program(const char* filename) {
    File f = SD.open(filename, FILE_READ); 
    if (!f) {
        Serial.print("Failed to load: ");
        Serial.println(filename);
        return;
    }

    lua_program.clear(); 
    int line_num = 10;   

    while (f.available()) {
        String l = f.readStringUntil('\n');
        // Windowsの改行コード(\r\n)対策
        if (l.length() > 0 && l[l.length() - 1] == '\r') {
            l.remove(l.length() - 1);
        }
        
        lua_program[line_num] = l.c_str();
        line_num += 10;
    }
    f.close();
    Serial.println("Loaded.");
}
