-- startup.lua
print("Loading config.lua...")

local content = sd_read("config.lua")
local config = { show_ip = true } -- デフォルト設定

if content then
    -- 文字列を Lua コードとして評価し，return されたテーブルを取得
    local chunk = load(content)
    if chunk then
        config = chunk()
        print("Config loaded successfully.")
    end
end

-- LCD の初期化と動的表示
lcd_init()
lcd_clear()
lcd_setCursor(0, 0)
lcd_print("uecsOS System")

local row = 1

if config.show_ip then
    lcd_setCursor(0, row)
    lcd_print("IP: " .. my_ip)
    row = row + 1
end

if config.show_status then
    lcd_setCursor(0, row)
    lcd_print("Status: ONLINE")
    row = row + 1
end

if config.show_version then
    lcd_setCursor(0, row)
    lcd_print("Ver: 2.0.0-Lua")
end
