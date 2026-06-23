#include "lua_functions.h"
#include <USBHost_t36.h>
#include "USB_FT232H_GPIO.h" // 【追加】新規ドライバをインクルード

#define LUA_HW_USB_VERSION "0.0.3"

// USBホストとシリアルデバイスの定義
USBHost myusb;
USBHub hub1(myusb);

// 【変更】標準のシリアルドライバを無効化し、新規ドライバを有効化
// USBSerial_BigBuffer userial(myusb); 
USB_FT232H_GPIO myftdi(myusb); 

extern "C" {
    // 5V電源のON/OFFを制御する関数
    int l_usb_power(lua_State *L) {
        bool enable = lua_toboolean(L, 1);
        IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_40 = 5;
        IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_40 = 0x0008;
        GPIO8_GDIR |= (1 << 26);
        if (enable) {
            GPIO8_DR_SET = (1 << 26);
        } else {
            GPIO8_DR_CLEAR = (1 << 26);
        }
        return 0;
    }

    // Lua引数: (baudrate)
    int l_usb_begin(lua_State *L) {
        int baud = (int)luaL_optinteger(L, 1, 115200);
        myusb.begin();
        // userial.begin(baud); // 【変更】一旦コメントアウト
        lua_pushboolean(L, true);
        return 1;
    }

    // Lua引数: (string)
    int l_usb_write(lua_State *L) {
        const char* data = luaL_checkstring(L, 1);
        /* 【変更】一旦コメントアウト
        if (userial) {
            userial.write(data);
            lua_pushboolean(L, true);
        } else {
            lua_pushboolean(L, false);
        }
        */
        lua_pushboolean(L, false);
        return 1;
    }

    // Lua戻り値: (string) 受信データ、データがなければ空文字列
    int l_usb_read(lua_State *L) {
        /* 【変更】一旦コメントアウト
        if (userial && userial.available()) {
            String incoming = "";
            while (userial.available()) {
                incoming += (char)userial.read();
            }
            lua_pushstring(L, incoming.c_str());
        } else {
            lua_pushstring(L, ""); // データなし
        }
        */
        lua_pushstring(L, "");
        return 1;
    }

    static const struct luaL_Reg usb_funcs[] = {
        {"begin", l_usb_begin},
        {"write", l_usb_write},
        {"read",  l_usb_read},
        {"power", l_usb_power},
        {NULL, NULL}
    };

    int luaopen_usb(lua_State *L) {
        luaL_newlib(L, usb_funcs);
        return 1;
    }
}