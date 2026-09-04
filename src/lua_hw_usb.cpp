#include "lua_functions.h"
#include <USBHost_t36.h>
#include "USB_FT232H_MPSSE.h"
#include <string.h>
#include <ctype.h>

#define LUA_HW_USB_VERSION "0.1.0"

#define MAX_FT232H_DEVICES 5

USBHost myusb;

FT232H_MPSSE ft232h_0(myusb);
FT232H_MPSSE ft232h_1(myusb);
FT232H_MPSSE ft232h_2(myusb);
FT232H_MPSSE ft232h_3(myusb);
FT232H_MPSSE ft232h_4(myusb);

USBHub hub0(myusb);
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHub hub3(myusb);
USBHub hub4(myusb);

FT232H_MPSSE* ft_devices[MAX_FT232H_DEVICES] = {
    &ft232h_0, &ft232h_1, &ft232h_2, &ft232h_3, &ft232h_4
};

extern "C" {
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

    int l_usb_isConnected(lua_State *L) {
        int dev_idx = luaL_optinteger(L, 1, 0);
        if (dev_idx < 0 || dev_idx >= MAX_FT232H_DEVICES) {
            lua_pushboolean(L, false);
            return 1;
        }
        lua_pushboolean(L, ft_devices[dev_idx]->isReady());
        return 1;
    }

    // FT232H ピン単位のON/OFF (例: usb.ft_pin(0, "d0", true))
    int l_usb_ft_pin(lua_State *L) {
        int dev_idx = luaL_checkinteger(L, 1);
        const char *pin_str = luaL_checkstring(L, 2);
        bool state = lua_toboolean(L, 3);

        if (dev_idx < 0 || dev_idx >= MAX_FT232H_DEVICES || !ft_devices[dev_idx]->isReady()) {
            lua_pushboolean(L, false);
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
        lua_pushboolean(L, false);
        return 1;
    }

    // 【新規】非ブロッキング・パルス制御 (例: usb.ft_pulse(0, "d0", 3000))
    int l_usb_ft_pulse(lua_State *L) {
        int dev_idx = luaL_checkinteger(L, 1);
        const char *pin_str = luaL_checkstring(L, 2);
        uint32_t duration_ms = (uint32_t)luaL_checkinteger(L, 3);

        if (dev_idx < 0 || dev_idx >= MAX_FT232H_DEVICES || !ft_devices[dev_idx]->isReady()) {
            lua_pushboolean(L, false);
            return 1;
        }

        if (strlen(pin_str) >= 2) {
            char port = tolower(pin_str[0]);
            int bit = pin_str[1] - '0';

            if (bit >= 0 && bit <= 7 && (port == 'd' || port == 'c')) {
                bool ok = ft_devices[dev_idx]->pinPulse(port, bit, duration_ms);
                lua_pushboolean(L, ok);
                return 1;
            }
        }
        lua_pushboolean(L, false);
        return 1;
    }

    int l_usb_ft_write_all(lua_State *L) {
        int dev_idx = luaL_checkinteger(L, 1);
        uint8_t adbus_val = (uint8_t)luaL_checkinteger(L, 2);
        uint8_t acbus_val = (uint8_t)luaL_checkinteger(L, 3);
        
        if (dev_idx < 0 || dev_idx >= MAX_FT232H_DEVICES || !ft_devices[dev_idx]->isReady()) {
            lua_pushboolean(L, false);
            return 1;
        }

        ft_devices[dev_idx]->setAll(adbus_val, acbus_val);
        lua_pushboolean(L, true);
        return 1;
    }

    // 【新規】物理トポロジパスの取得 (例: usb.topology(0))
    int l_usb_topology(lua_State *L) {
        int index = luaL_optinteger(L, 1, 0);
        if (index < 0 || index >= MAX_FT232H_DEVICES || ft_devices[index] == nullptr) {
            lua_pushstring(L, "Disconnected");
            return 1;
        }
        String path = ft_devices[index]->getTopologyPath();
        lua_pushstring(L, path.c_str());
        return 1;
    }

    int l_usb_info(lua_State *L) {
        lua_newtable(L);
        int t_index = 1;
        
        for (int i = 0; i < MAX_FT232H_DEVICES; i++) {
            lua_pushinteger(L, t_index++);
            lua_newtable(L);
            
            lua_pushstring(L, "id");
            lua_pushinteger(L, i);
            lua_settable(L, -3);
            
            lua_pushstring(L, "ready");
            lua_pushboolean(L, ft_devices[i]->isReady());
            lua_settable(L, -3);
            
            lua_pushstring(L, "vid");
            lua_pushinteger(L, ft_devices[i]->getVID());
            lua_settable(L, -3);
            
            lua_pushstring(L, "pid");
            lua_pushinteger(L, ft_devices[i]->getPID());
            lua_settable(L, -3);

            lua_pushstring(L, "mfg");
            lua_pushstring(L, ft_devices[i]->getManufacturer().c_str());
            lua_settable(L, -3);

            lua_pushstring(L, "product");
            lua_pushstring(L, ft_devices[i]->getProduct().c_str());
            lua_settable(L, -3);

            lua_pushstring(L, "serial");
            lua_pushstring(L, ft_devices[i]->getSerial().c_str());
            lua_settable(L, -3);

            lua_pushstring(L, "path");
            lua_pushstring(L, ft_devices[i]->getTopologyPath().c_str());
            lua_settable(L, -3);

            lua_pushstring(L, "address");
            lua_pushinteger(L, ft_devices[i]->getAddress());
            lua_settable(L, -3);
            
            lua_pushstring(L, "hub_addr");
            lua_pushinteger(L, ft_devices[i]->getHubAddress());
            lua_settable(L, -3);
            
            lua_pushstring(L, "hub_port");
            lua_pushinteger(L, ft_devices[i]->getHubPort());
            lua_settable(L, -3);

            lua_settable(L, -3); 
        }
        return 1;
    }

    // テーブル登録
    static const struct luaL_Reg usb_funcs[] = {
        {"begin", l_usb_begin},
        {"write", l_usb_write},
        {"read",  l_usb_read},
        {"power", l_usb_power},
        {"isConnected", l_usb_isConnected},
        {"ft_pin", l_usb_ft_pin},
        {"ft_pulse", l_usb_ft_pulse},       // 追加
        {"ft_write_all", l_usb_ft_write_all},
        {"topology", l_usb_topology},       // 追加
        {"info", l_usb_info},
        {NULL, NULL}
    };

    int luaopen_usb(lua_State *L) {
        luaL_newlib(L, usb_funcs);
        return 1;
    }
}