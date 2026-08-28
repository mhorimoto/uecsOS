#include "lua_functions.h"
#include <USBHost_t36.h>
#include "USB_FT232H_MPSSE.h"
#include <string.h>
#include <ctype.h>

#define LUA_HW_USB_VERSION "0.0.9"

// 同時接続を許可するFT232Hの最大台数 (5台で80個リレーを制御可能)
#define MAX_FT232H_DEVICES 5

// USBホストとハブの定義
USBHost myusb;

// 【変更】ドライバインスタンスを最大台数分、静的に確保する
FT232H_MPSSE ft232h_0(myusb);
FT232H_MPSSE ft232h_1(myusb);
FT232H_MPSSE ft232h_2(myusb);
FT232H_MPSSE ft232h_3(myusb);
FT232H_MPSSE ft232h_4(myusb);

USBHub hub0(myusb);
USBHub hub1(myusb);
USBHub hub2(myusb); // カスケードされる2段目のHUB用に追加
USBHub hub3(myusb); // 予備（7ポートHUBなどの内部カスケード対応）
USBHub hub4(myusb); // 予備

// プログラムから扱いやすいようにポインタの配列にまとめる
FT232H_MPSSE* ft_devices[MAX_FT232H_DEVICES] = {
    &ft232h_0, &ft232h_1, &ft232h_2, &ft232h_3, &ft232h_4
};

extern "C" {
    // 5V電源のON/OFF
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
        lua_pushboolean(L, true);
        return 1;
    }

    int l_usb_write(lua_State *L) {
        lua_pushboolean(L, false);
        return 1;
    }

    int l_usb_read(lua_State *L) {
        lua_pushstring(L, "");
        return 1;
    }

    // ============================================================
    // 接続状態の確認（引数でデバイスIDを指定。省略時は0）
    // Lua引数: (0) または 省略
    // ============================================================
    int l_usb_isConnected(lua_State *L) {
        int dev_idx = luaL_optinteger(L, 1, 0); // デフォルトは 0番デバイス
        
        if (dev_idx < 0 || dev_idx >= MAX_FT232H_DEVICES) {
            lua_pushboolean(L, false);
            return 1;
        }

        lua_pushboolean(L, ft_devices[dev_idx]->isReady());
        return 1;
    }

    // ============================================================
    // FT232H ピン単位のON/OFF
    // Lua引数: (デバイスID, "d0", true) 
    // ============================================================
    int l_usb_ft_pin(lua_State *L) {
        int dev_idx = luaL_checkinteger(L, 1);
        const char *pin_str = luaL_checkstring(L, 2);
        bool state = lua_toboolean(L, 3);

        if (dev_idx < 0 || dev_idx >= MAX_FT232H_DEVICES) {
            lua_pushboolean(L, false); // 不正なデバイスID
            return 1;
        }

        if (!ft_devices[dev_idx]->isReady()) {
            lua_pushboolean(L, false); // デバイス未接続
            return 1;
        }

        if (strlen(pin_str) >= 2) {
            char port = tolower(pin_str[0]);
            int bit = pin_str[1] - '0';

            if (bit >= 0 && bit <= 7) {
                if (port == 'd') {
                    ft_devices[dev_idx]->writeADBUSBit(bit, state);
                    lua_pushboolean(L, true);
                    return 1;
                } else if (port == 'c') {
                    ft_devices[dev_idx]->writeACBUSBit(bit, state);
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
    // Lua引数: (デバイスID, 0x00, 0xFF)
    // ============================================================
    int l_usb_ft_write_all(lua_State *L) {
        int dev_idx = luaL_checkinteger(L, 1);
        uint8_t adbus_val = (uint8_t)luaL_checkinteger(L, 2);
        uint8_t acbus_val = (uint8_t)luaL_checkinteger(L, 3);
        
        if (dev_idx < 0 || dev_idx >= MAX_FT232H_DEVICES) {
            lua_pushboolean(L, false);
            return 1;
        }

        if (ft_devices[dev_idx]->isReady()) {
            ft_devices[dev_idx]->setAll(adbus_val, acbus_val);
            lua_pushboolean(L, true);
        } else {
            lua_pushboolean(L, false);
        }
        return 1;
    }

    // ============================================================
    // USBデバイスの接続情報をLuaのテーブルとして返す
    // Lua戻り値: { {id=0, ready=true, vid=1027, pid=24596}, ... }
    // ============================================================
    int l_usb_info(lua_State *L) {
        lua_newtable(L); // 戻り値となる配列（全体テーブル）
        int t_index = 1;
        
        for (int i = 0; i < MAX_FT232H_DEVICES; i++) {
            lua_pushinteger(L, t_index++); // Lua配列のインデックス（1始まり）
            
            lua_newtable(L); // 各デバイス情報を格納する子テーブル
            
            // デバイス番号（0〜3）
            lua_pushstring(L, "id");
            lua_pushinteger(L, i);
            lua_settable(L, -3);
            
            // 接続状態（true / false）
            lua_pushstring(L, "ready");
            lua_pushboolean(L, ft_devices[i]->isReady());
            lua_settable(L, -3);
            
            // Vendor ID (VID)
            lua_pushstring(L, "vid");
            lua_pushinteger(L, ft_devices[i]->getVID());
            lua_settable(L, -3);
            
            // Product ID (PID)
            lua_pushstring(L, "pid");
            lua_pushinteger(L, ft_devices[i]->getPID());
            lua_settable(L, -3);

            // Manufacturer
            lua_pushstring(L, "mfg");
            lua_pushstring(L, ft_devices[i]->getManufacturer().c_str());
            lua_settable(L, -3);

            // Product
            lua_pushstring(L, "product");
            lua_pushstring(L, ft_devices[i]->getProduct().c_str());
            lua_settable(L, -3);

            // Serial Number (シリアル文字列)
            lua_pushstring(L, "serial");
            lua_pushstring(L, ft_devices[i]->getSerial().c_str());
            lua_settable(L, -3);

            // 物理接続トポロジ情報
            lua_pushstring(L, "address");
            lua_pushinteger(L, ft_devices[i]->getAddress());
            lua_settable(L, -3);
            
            lua_pushstring(L, "hub_addr");
            lua_pushinteger(L, ft_devices[i]->getHubAddress());
            lua_settable(L, -3);
            
            lua_pushstring(L, "hub_port");
            lua_pushinteger(L, ft_devices[i]->getHubPort());
            lua_settable(L, -3);

            // 子テーブルを全体テーブルにセット
            lua_settable(L, -3); 
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
        {"info", l_usb_info},
        {NULL, NULL}
    };
    int luaopen_usb(lua_State *L) {
        luaL_newlib(L, usb_funcs);
        return 1;
    }
}
