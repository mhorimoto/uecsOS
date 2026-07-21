#ifndef LUA_EXECUTOR_H
#define LUA_EXECUTOR_H

#include <Arduino.h>
#include <map>
#include <string>
#include "lua_functions.h" // lua_State等の型を利用するため

// グローバルなLuaプログラム保持マップ（従来の対話編集用）
extern std::map<int, std::string> lua_program;

// ============================================================
// 永続Lua VM（スケジューラ専用）
//   UECS標準インターバル(1sec/10sec/1min)で呼び出される
//   exec1sec()/exec10sec()/exec1min() を保持し続けるVM
//   毎回破棄される対話実行用VMとは別物
// ============================================================
extern lua_State *g_lua_main;

// 永続VMの初期化（setup()で1回だけ呼ぶ）
// default_filename: ポインタファイルが無い場合に使うデフォルトのLuaファイル名
//   起動時、SD上に /active_scheduler.txt が存在すれば、
//   そちらに書かれたファイル名を優先して読み込む（前回選択の復元）
void init_persistent_lua(const char* default_filename);

// 永続VM上の予約関数(exec1sec等)を呼び出す
// 関数が定義されていない場合は何もせず正常終了する
void call_scheduled_function(const char* fname);

// ============================================================
// 実行中の永続VM切り替え（SCHEDコマンド等から呼ぶ）
//   1. 現在の永続VMを破棄（lua_close）
//   2. 指定ファイルを新しい永続VMとして読み込み・実行
//   3. 成功した場合、選択内容を /active_scheduler.txt に保存
//      （次回起動時も同じファイルが自動的に使われる）
//
//   ※ FT232Hのパルスタイマーは永続VMとは別のC++オブジェクトで
//     管理されているため、VMを切り替えても動作中のパルスは
//     中断されず、時間が来れば正しくOFFになる
//
//   戻り値: 読み込み・実行に成功した場合true
// ============================================================
bool reload_persistent_lua(const char* filename);

// 現在アクティブなスケジューラファイル名を取得（表示用）
String get_active_scheduler_filename();

// --- 従来API（対話実行・使い捨てVM用） ---
void save_lua_program(const char* filename);
void load_lua_program(const char* filename);
void execute_lua_file(const char* filename);
void lua_os_hook(lua_State *L, lua_Debug *ar);
void stop_persistent_lua();
#endif
