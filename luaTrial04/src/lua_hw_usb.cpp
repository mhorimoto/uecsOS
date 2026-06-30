#include "lua_functions.h"
#include <USBHost_t36.h>
#include "USB_FT232H_MPSSE.h" 
#include <string.h>
#include <ctype.h>

#define LUA_HW_USB_VERSION "0.0.5"

// USBホストとハブの定義
USBHost myusb;
USBHub hub1(myusb);

FT232H_MPSSE ft232h(myusb); 

extern "C" {
    // 5V電源のON/OFFを制御する関数 (ロードスイッチ直叩き)
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

    // USBホストコアの起動
    int l_usb_begin(lua_State *L) {
        myusb.begin();
        lua_pushboolean(L, true);
        return 1;
    }

    // (従来のシリアル送受信用：現在はMPSSE専用のため無効化)
    int l_usb_write(lua_State *L) {
        lua_pushboolean(L, false);
        return 1;
    }

    int l_usb_read(lua_State *L) {
        lua_pushstring(L, "");
        return 1;
    }

    // --- 接続状態の確認 ---
    int l_usb_isConnected(lua_State *L) {
        // 新ドライバの isReady() メソッドを使用
        lua_pushboolean(L, ft232h.isReady());
        return 1;
    }

    // ============================================================
    // FT232H ピン単位のON/OFF
    // Lua引数: ("d0", true) や ("c7", false)
    // ============================================================
    int l_usb_ft_pin(lua_State *L) {
        const char *pin_str = luaL_checkstring(L, 1);
        bool state = lua_toboolean(L, 2);

        if (!ft232h.isReady()) {
            lua_pushboolean(L, false); // デバイス未接続
            return 1;
        }

        if (strlen(pin_str) >= 2) {
            char port = tolower(pin_str[0]);
            int bit = pin_str[1] - '0';

            if (bit >= 0 && bit <= 7) {
                if (port == 'd') {
                    ft232h.writeADBUSBit(bit, state);
                    lua_pushboolean(L, true);
                    return 1;
                } else if (port == 'c') {
                    ft232h.writeACBUSBit(bit, state);
                    lua_pushboolean(L, true);
                    return 1;
                }
            }
        }
        
        lua_pushboolean(L, false); // パラメータエラー
        return 1;
    }

    // ============================================================
    // FT232H 全ピンの一括制御
    // Lua引数: (0x00, 0xFF) ※ADBUS, ACBUSのバイト値（負論理値そのまま）
    // ============================================================
    int l_usb_ft_write_all(lua_State *L) {
        uint8_t adbus_val = (uint8_t)luaL_checkinteger(L, 1);
        uint8_t acbus_val = (uint8_t)luaL_checkinteger(L, 2);
        
        if (ft232h.isReady()) {
            ft232h.setAll(adbus_val, acbus_val);
            lua_pushboolean(L, true);
        } else {
            lua_pushboolean(L, false);
        }
        return 1;
    }

    // 関数テーブルの登録
    static const struct luaL_Reg usb_funcs[] = {
        {"begin", l_usb_begin},
        {"write", l_usb_write},
        {"read",  l_usb_read},
        {"power", l_usb_power},
        {"isConnected", l_usb_isConnected},
        {"ft_pin", l_usb_ft_pin},
        {"ft_write_all", l_usb_ft_write_all},
        {NULL, NULL}
    };

    int luaopen_usb(lua_State *L) {
        luaL_newlib(L, usb_funcs);
        return 1;
    }
}
