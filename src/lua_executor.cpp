#include "lua_executor.h"
#include <SD.h>
#include <NativeEthernet.h>
#include "USB_FT232H_MPSSE.h"
// ============================================================
// 【追記】Luaから呼び出される FT232H トポロジ取得用ラッパー
// ============================================================
static int l_ft232h_get_topology(lua_State *L) {
    int index = luaL_checkinteger(L, 1);
    if (index < 0 || index >= MAX_FT232H_DEVICES) {
        lua_pushstring(L, "Disconnected");
        return 1;
    }
    if (ft_devices[index] == nullptr) {
        lua_pushstring(L, "Disconnected");
        return 1;
    }
    String path = ft_devices[index]->getTopologyPath();
    lua_pushstring(L, path.c_str());
    return 1;
}

// ============================================================
// 【追記】ラッパー関数のLua VMへの登録関数
// ============================================================
void register_lua_usb_topology(lua_State *L) {
    lua_register(L, "ft232h_get_topology", l_ft232h_get_topology);
}
// ============================================================

// main.cpp に残るUI関数とネットワークエンジンの外部参照
extern void update_os_display(); 
extern void execute_uecs_transmission();
extern void process_network_manager();
extern void process_incoming_uecs();
extern void check_uecs_lifespan();
extern void run_os_background_tasks();

// プログラム保持用のマップの実体（対話編集用・従来のまま）
std::map<int, std::string> lua_program;

// ============================================================
// 永続Lua VM の実体
//   スケジューラ(exec1sec/exec10sec/exec1min)専用
// ============================================================
lua_State *g_lua_main = nullptr;

// 現在アクティブなスケジューラのファイル名（表示・記録用）
static String g_active_scheduler_filename = "";

// 「前回選択したファイル名」を記録するポインタファイル
#define ACTIVE_SCHEDULER_POINTER_FILE "/active_scheduler.txt"

// --- 究極の要塞化：Lua強制フック関数 ---
// 対話実行用の一時VMと、永続VM（スケジューラ）の両方に設定される
void lua_os_hook(lua_State *L, lua_Debug *ar) {
    // --- 緊急停止（Halt）シグナルの監視 ---
    while (Serial.available() > 0) {
        char c = Serial.read();
        if (c == 3 || c == 27) {
            Serial.println("\n[OS] Emergency Halt Triggered!");
            luaL_error(L, "Script halted by OS Interrupt"); 
        }
    }
    yield();
    run_os_background_tasks();
}

// ============================================================
// 永続VMへのファイル読み込み・実行（内部共通処理）
//   呼び出し元: init_persistent_lua() / reload_persistent_lua()
//   既存のg_lua_mainがあれば破棄してから新しいVMを生成する
//
//   ★ FT232Hのタイマー(_timers[])はg_lua_mainとは別オブジェクトの
//     ためVM破棄の影響を受けない。動作中のパルスは中断されない。
// ============================================================
static bool _persistent_vm_load(const char* filename) {
    if (g_lua_main != nullptr) {
        lua_close(g_lua_main);
        g_lua_main = nullptr;
        Serial.println("[Lua Persistent] Previous VM closed.");
    }

    if (!SD.exists(filename)) {
        Serial.print("[Lua Persistent] File not found: ");
        Serial.println(filename);
        return false;
    }

    g_lua_main = luaL_newstate();
    luaL_openlibs(g_lua_main);
    register_lua_functions(g_lua_main);
    // 【追記】永続VMへUSBトポロジ取得関数を登録
    register_lua_usb_topology(g_lua_main);
    lua_sethook(g_lua_main, lua_os_hook, LUA_MASKCOUNT, 10000);

    // 環境変数のセットアップ（対話実行用VMと同様）
    IPAddress ip = Ethernet.localIP();
    String ipStr = String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
    lua_pushstring(g_lua_main, ipStr.c_str());
    lua_setglobal(g_lua_main, "my_ip");

    File f = SD.open(filename);
    String script = "";
    while (f.available()) script += (char)f.read();
    f.close();

    bool ok = (luaL_dostring(g_lua_main, script.c_str()) == LUA_OK);
    if (!ok) {
        Serial.print("[Lua Persistent] Load Error: ");
        Serial.println(lua_tostring(g_lua_main, -1));
        lua_pop(g_lua_main, 1);
        // ★ 構文エラーでもVM自体は残す（exec関数が未定義のまま起動継続）
        //   スケジューラ全体がクラッシュするよりは安全側の判断
    } else {
        Serial.print("[Lua Persistent] Scheduler loaded: ");
        Serial.println(filename);
    }

    g_active_scheduler_filename = filename;
    return ok;
}

// ============================================================
// 永続Lua VMの初期化（setup()で1回だけ呼ぶ）
//   /active_scheduler.txt が存在すれば、前回選択したファイルを優先
//   存在しなければ default_filename を使用する
// ============================================================
void init_persistent_lua(const char* default_filename) {
    if (g_lua_main != nullptr) {
        Serial.println("[Lua Persistent] Already initialized. Skipping.");
        return;
    }

    String target = default_filename;

    if (SD.exists(ACTIVE_SCHEDULER_POINTER_FILE)) {
        File pf = SD.open(ACTIVE_SCHEDULER_POINTER_FILE, FILE_READ);
        if (pf) {
            String saved = pf.readStringUntil('\n');
            saved.trim();
            pf.close();
            if (saved.length() > 0) {
                target = saved;
                Serial.print("[Lua Persistent] Restoring previous selection: ");
                Serial.println(target);
            }
        }
    }

    _persistent_vm_load(target.c_str());
}

// ============================================================
// 実行中の永続VM切り替え（SCHEDコマンド等から呼ぶ）
//   成功時は選択内容を /active_scheduler.txt に保存し、
//   次回起動時も同じファイルが自動的に使われるようにする
// ============================================================
bool reload_persistent_lua(const char* filename) {
    bool ok = _persistent_vm_load(filename);
    if (ok) {
        File pf = SD.open(ACTIVE_SCHEDULER_POINTER_FILE, FILE_WRITE);
        if (pf) {
            pf.truncate();
            pf.print(filename);
            pf.close();
        }
    }
    return ok;
}

String get_active_scheduler_filename() {
    return g_active_scheduler_filename;
}

// ============================================================
// 永続VM上の予約関数を呼び出す
//   exec1sec() / exec10sec() / exec1min() のいずれかを想定
//   関数が定義されていなければ何もせず正常終了（エラーにしない）
// ============================================================
void call_scheduled_function(const char* fname) {
    if (g_lua_main == nullptr) return;

    lua_getglobal(g_lua_main, fname);
    if (!lua_isfunction(g_lua_main, -1)) {
        lua_pop(g_lua_main, 1);   // 未定義なら黙ってスキップ
        return;
    }

    if (lua_pcall(g_lua_main, 0, 0, 0) != LUA_OK) {
        Serial.print("[Lua Scheduled Error] ");
        Serial.print(fname);
        Serial.print(": ");
        Serial.println(lua_tostring(g_lua_main, -1));
        lua_pop(g_lua_main, 1);   // エラーメッセージをスタックから除去
        // ★ 意図的にVMは破棄しない。1回のエラーでスケジューラ全体を
        //   止めないため（次回の呼び出しでまた挑戦する）
    }
}
// ============================================================
// 永続VMの明示的停止（SCHED STOPコマンド等から呼ぶ）
//   新規のexec1sec/10sec/1min呼び出しを完全に止める
//   ※ 既に物理出力がONになっている場合、それ自体は変化しない
//     （呼び出し側でallOff()等と組み合わせること）
// ============================================================
void stop_persistent_lua() {
    if (g_lua_main != nullptr) {
        lua_close(g_lua_main);
        g_lua_main = nullptr;
        Serial.println("[Lua Persistent] Stopped.");
    }
}

// --- Luaファイル実行部（対話実行・使い捨てVM／従来のまま） ---
void execute_lua_file(const char* filename) {
    if (!SD.exists(filename)) return;
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    register_lua_functions(L);
    // 【追記】対話実行・使い捨てVMへUSBトポロジ取得関数を登録
    register_lua_usb_topology(L);
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
        f.truncate();
        for (auto const& [num, code] : lua_program) {
            f.println(code.c_str());
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
        if (l.length() > 0 && l[l.length() - 1] == '\r') {
            l.remove(l.length() - 1);
        }
        lua_program[line_num] = l.c_str();
        line_num += 10;
    }
    f.close();
    Serial.println("Loaded.");
}
