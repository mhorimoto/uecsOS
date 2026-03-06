#include "lua_functions.h"
#include <USBHost_t36.h>

// USBホストとシリアルデバイスの定義
USBHost myusb;
USBHub hub1(myusb);
USBSerial_BigBuffer userial(myusb); // FT232Hとの通信用

// Lua引数: (baudrate)
int l_usb_begin(lua_State *L) {
    int baud = (int)luaL_optinteger(L, 1, 115200);
    myusb.begin();
    userial.begin(baud);
    lua_pushboolean(L, true);
    return 1;
}

// Lua引数: (string)
int l_usb_write(lua_State *L) {
    const char* data = luaL_checkstring(L, 1);
    if (userial) {
        userial.write(data);
        lua_pushboolean(L, true);
    } else {
        lua_pushboolean(L, false);
    }
    return 1;
}

// Lua戻り値: (string) 受信データ、データがなければ空文字列
int l_usb_read(lua_State *L) {
    if (userial && userial.available()) {
        String incoming = "";
        while (userial.available()) {
            incoming += (char)userial.read();
        }
        lua_pushstring(L, incoming.c_str());
    } else {
        lua_pushstring(L, ""); // データなし
    }
    return 1;
}

// ※将来的にMPSSEコマンドを送ってGPIOを操作する関数を追加予定