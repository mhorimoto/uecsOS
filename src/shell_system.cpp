#include "shell_system.h"
#include <SD.h>
#include <map>
#include <string>
#include "lua_functions.h"
#include "lua_executor.h"
#include "USB_FT232H_MPSSE.h"

// 出力先として Print& out を受け取るよう変更
void execute_shell_command(String line, Print& out) {
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
        int line_num = first_word.toInt();
        if (first_space > 0) {
            lua_program[line_num] = line.substring(first_space + 1).c_str();
        } else {
            lua_program.erase(line_num);
        }
        return;
    }

    // 2. コマンド判定
    String cmd = line;
    cmd.toUpperCase();

    if (cmd == "RUN") {
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
            out.println(lua_tostring(L, -1)); // Serial から out へ変更
        }
        lua_close(L);
        out.println("Ok");
    } else if (cmd == "LIST") {
        for (auto const& [num, code] : lua_program) {
            out.printf("%d %s\n", num, code.c_str());
        }
    } else if (cmd == "NEW") {
        lua_program.clear();
        out.println("Ok");
    } else if (cmd == "DIR") {
        File dir = SD.open("/");
        if (!dir) {
            out.println("Failed to open SD root.");
        } else {
            while (true) {
                File entry = dir.openNextFile();
                if (!entry) break;
                out.print(entry.name());
                if (entry.isDirectory()) {
                    out.println("/");
                } else {
                    out.print("\t");
                    out.print(entry.size());
                    out.println(" bytes");
                }
                entry.close();
            }
            dir.close();
        }
        out.println("Ok");
    } else if (cmd.startsWith("SAVE ")) {
        String filename = line.substring(5);
        filename.trim();
        save_lua_program(filename.c_str());
        out.println("Ok"); // UDP等の通信元へ完了を通知するために追加
    } else if (cmd.startsWith("LOAD ")) {
        String filename = line.substring(5);
        filename.trim();
        load_lua_program(filename.c_str());
        out.println("Ok"); // UDP等の通信元へ完了を通知するために追加
    } else if (cmd == "SCHED") {
        out.print("Active scheduler: ");
        out.println(get_active_scheduler_filename());
    } else if (cmd == "SCHED STOP") {
        stop_persistent_lua();
        for (int i = 0; i < MAX_FT232H_DEVICES; i++) {
            if (ft_devices[i]->isReady()) {
                ft_devices[i]->allOff();
            }
        }
        out.println("Ok - all relays forced OFF");
    } else if (cmd.startsWith("SCHED ")) {
        String filename = line.substring(6);
        filename.trim();
        if (filename.length() == 0) {
            out.println("Usage: SCHED <filename.lua>");
        } else if (reload_persistent_lua(filename.c_str())) {
            out.print("Ok - scheduler switched to ");
            out.println(filename);
        } else {
            out.print("Error - failed to load ");
            out.println(filename);
        }
    } else {
        // 即時実行モード
        lua_State *L = luaL_newstate();
        luaL_openlibs(L);
        register_lua_functions(L);
        lua_sethook(L, lua_os_hook, LUA_MASKCOUNT, 10000);
        if (luaL_dostring(L, line.c_str()) != LUA_OK) {
            out.println(lua_tostring(L, -1));
        } else {
            out.println("Ok"); // UDPへの正常終了通知のため追加
        }
        lua_close(L);
    }
}

// 外部に公開する従来のシリアル処理関数
void process_serial_shell() {
    static String serialBuffer = "";
    if (Serial.available() > 0) {
        char c = Serial.read();
        if (c == '\b' || c == 127) {
            if (serialBuffer.length() > 0) {
                serialBuffer.remove(serialBuffer.length() - 1);
                Serial.print("\b \b");
            }
        } else if (c == '\n' || c == '\r') {
            if (serialBuffer.length() > 0) {
                Serial.println();
                execute_shell_command(serialBuffer, Serial); // Serial を Print& として渡す
                serialBuffer = "";
            }
        } else if (c >= 32 && c <= 126) {
            serialBuffer += c;
            Serial.print(c);
        }
    }
}