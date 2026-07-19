#include "serial_shell.h"
#include <SD.h>
#include <map>
#include <string>
#include "lua_functions.h"
#include "lua_executor.h"

static void handle_serial_input(String line) {
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
        // 行番号あり：プログラムの登録（対話編集用・従来のまま）
        int line_num = first_word.toInt();
        if (first_space > 0) {
            lua_program[line_num] = line.substring(first_space + 1).c_str(); 
        } else {
            lua_program.erase(line_num); // 行番号のみは行削除
        }
        return;
    }

    // 2. コマンド判定
    String cmd = line;
    cmd.toUpperCase();

    if (cmd == "RUN") {
        // 対話編集バッファ(lua_program)を使い捨てVMで実行（従来のまま）
        std::string full_script = "";
        for (auto const& [num, code] : lua_program) {
            full_script += code;
            full_script += "\n";
        }
        lua_State *L = luaL_newstate();
        luaL_openlibs(L);
        register_lua_functions(L);
        lua_sethook(L, lua_os_hook, LUA_MASKCOUNT, 10000);
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

    // ============================================================
    // ★ SCHED コマンド（永続VM＝スケジューラの切り替え専用）
    //   既存のLOAD/RUN（対話編集用の使い捨てVM）とは別系統
    //
    //   SCHED            → 現在アクティブなスケジューラファイル名を表示
    //   SCHED progA.lua  → 永続VMを破棄・再生成し、progA.luaを即座に有効化
    //                       成功時は /active_scheduler.txt に選択を記録し、
    //                       次回起動時も自動的に同じファイルが使われる
    //
    //   ※ 稼働中のFT232Hパルスはこの切り替えで中断されない
    //     （タイマー管理はLua VMと独立したC++オブジェクトのため）
    // ============================================================
    } else if (cmd == "SCHED") {
        Serial.print("Active scheduler: ");
        Serial.println(get_active_scheduler_filename());
    } else if (cmd.startsWith("SCHED ")) {
        String filename = line.substring(6);
        filename.trim();
        if (filename.length() == 0) {
            Serial.println("Usage: SCHED <filename.lua>");
        } else if (reload_persistent_lua(filename.c_str())) {
            Serial.print("Ok - scheduler switched to ");
            Serial.println(filename);
        } else {
            Serial.print("Error - failed to load ");
            Serial.println(filename);
        }
    } else {
        // 即時実行モード（1行だけのコマンドをその場で実行）
        lua_State *L = luaL_newstate();
        luaL_openlibs(L);
        register_lua_functions(L);
        lua_sethook(L, lua_os_hook, LUA_MASKCOUNT, 10000);
        if (luaL_dostring(L, line.c_str()) != LUA_OK) {
            Serial.println(lua_tostring(L, -1));
        }
        lua_close(L);
    }
}

// 外部に公開するシリアル処理関数
void process_serial_shell() {
    static String serialBuffer = "";
    if (Serial.available() > 0) {
        char c = Serial.read();
        if (c == '\b' || c == 127) { 
            // バックスペース処理
            if (serialBuffer.length() > 0) {
                serialBuffer.remove(serialBuffer.length() - 1);
                Serial.print("\b \b");
            }
        } else if (c == '\n' || c == '\r') {
            // エンターキー処理
            if (serialBuffer.length() > 0) {
                Serial.println();
                handle_serial_input(serialBuffer);
                serialBuffer = "";
            }
        } else if (c >= 32 && c <= 126) { 
            // 通常文字処理
            serialBuffer += c;
            Serial.print(c); 
        }
    }
}
