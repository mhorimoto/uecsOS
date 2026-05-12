-- hexedit.lua
-- uecsOS EEPROM Hex Editor Utility

function dump(start_addr)
    start_addr = start_addr or 0
    local len = eeprom.length()
    if start_addr < 0 or start_addr >= len then
        print("Error: Address out of range.")
        return
    end

    print(string.format("--- EEPROM DUMP (Base: 0x%04X) ---", start_addr))
    print("ADDR : 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F : ASCII")
    print("------------------------------------------------------------------")

    for row = 0, 15 do
        local base = start_addr + (row * 16)
        if base >= len then break end

        local hex_str = ""
        local asc_str = ""

        for col = 0, 15 do
            local addr = base + col
            if addr < len then
                local val = eeprom.read(addr)
                hex_str = hex_str .. string.format("%02X ", val)
                if val >= 32 and val <= 126 then
                    asc_str = asc_str .. string.char(val)
                else
                    asc_str = asc_str .. "."
                end
            else
                hex_str = hex_str .. "   "
            end
        end
        print(string.format("%04X : %s: %s", base, hex_str, asc_str))
    end
end

function edit(addr, v_type, value)
    if addr < 0 or addr >= eeprom.length() then
        print("Error: Address out of range.")
        return
    end

    if v_type == "c" then
        -- 1バイト (Char)
        eeprom.write(addr, value & 0xFF)
        print(string.format("Write [char] 0x%02X at 0x%04X", value & 0xFF, addr))
    elseif v_type == "i" then
        -- 4バイト (Int) : Teensy 4.1 (ARM) はリトルエンディアン
        eeprom.write(addr, value & 0xFF)
        eeprom.write(addr + 1, (value >> 8) & 0xFF)
        eeprom.write(addr + 2, (value >> 16) & 0xFF)
        eeprom.write(addr + 3, (value >> 24) & 0xFF)
        print(string.format("Write [int] %d at 0x%04X-0x%04X", value, addr, addr+3))
    else
        print("Error: Unknown type. Use 'c' or 'i'.")
    end
end

print("HexEditor Loaded! Commands:")
print("  dump(address)")
print("  edit(address, 'c'|'i', value)")
