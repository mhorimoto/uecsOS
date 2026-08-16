#include "lua_functions.h"

int l_sd_dir(lua_State *L) {
    const char* path = luaL_optstring(L, 1, "/");
    File dir = SD.open(path);
    lua_newtable(L);
    int index = 1;
    if (!dir || !dir.isDirectory()) return 1;

    while (true) {
        File entry = dir.openNextFile();
        if (!entry) break;
        String name = String(entry.name());
        if (entry.isDirectory()) name += "/";
        lua_pushstring(L, name.c_str());
        lua_rawseti(L, -2, index++);
        entry.close();
    }
    dir.close();
    return 1;
}

int l_sd_append(lua_State *L) {
    const char* filename = luaL_checkstring(L, 1);
    const char* text = luaL_checkstring(L, 2);
    File dataFile = SD.open(filename, FILE_WRITE);
    if (dataFile) {
        dataFile.println(text);
        dataFile.close();
        lua_pushboolean(L, true);
    } else {
        lua_pushboolean(L, false);
    }
    return 1;
}

int l_sd_read(lua_State *L) {
    const char* filename = luaL_checkstring(L, 1);
    File dataFile = SD.open(filename);
    if (dataFile) {
        String content = "";
        while (dataFile.available()) content += (char)dataFile.read();
        dataFile.close();
        lua_pushstring(L, content.c_str());
    } else {
        lua_pushnil(L);
    }
    return 1;
}