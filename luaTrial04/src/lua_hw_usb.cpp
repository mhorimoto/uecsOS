#include "lua_functions.h"
#include <USBHost_t36.h>
#define LUA_HW_USB_VERSION "0.0.2"

// USBホストとシリアルデバイスの定義
USBHost myusb;
USBHub hub1(myusb);
USBSerial_BigBuffer userial(myusb); // FT232Hとの通信用

extern "C" {
    // 5V電源のON/OFFを制御する関数
    int l_usb_power(lua_State *L) {
        // Lua側からの引数 (true=ON, false=OFF) を取得
        bool enable = lua_toboolean(L, 1);

        // EMC_40ピンをGPIOモード(ALT5)に設定
        IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_40 = 5;
        
        // パッドの電気的特性（スルーレートや駆動能力）を設定
        IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_40 = 0x0008;
        
        // GPIO8の26ビット目 (EMC_40に該当) を出力モードに設定
        GPIO8_GDIR |= (1 << 26);

        if (enable) {
            // HIGHを出力（ロードスイッチをONにして5V供給）
            GPIO8_DR_SET = (1 << 26);
        } else {
            // LOWを出力（5V供給を遮断）
            GPIO8_DR_CLEAR = (1 << 26);
        }

        return 0;
    }

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

// ※将来的にMPSSEコマンドを送ってGPIOを操作する関数を追加予定
}