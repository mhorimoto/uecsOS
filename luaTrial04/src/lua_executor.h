#ifndef LUA_EXECUTOR_H
#define LUA_EXECUTOR_H

#include <Arduino.h>
#include <map>
#include <string>
#include "lua_functions.h" // lua_State等の型を利用するため

// グローバルなLuaプログラム保持マップ
extern std::map<int, std::string> lua_program;

// Lua管理・実行用API
void save_lua_program(const char* filename);
void load_lua_program(const char* filename);
void execute_lua_file(const char* filename);
void lua_os_hook(lua_State *L, lua_Debug *ar);

#endif