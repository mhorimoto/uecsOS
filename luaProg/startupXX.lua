-- startup.lua
print("System booting...")
print("My IP address is: " .. my_ip)

-- LCDの初期化と表示
lcd_init()
lcd_clear()
lcd_setCursor(0, 0)
lcd_print("uecsOS Teensy 4.1")
lcd_setCursor(0, 1)
lcd_print("IP: " .. my_ip)
lcd_setCursor(0, 2)
lcd_print("Status: Online")
lcd_setCursor(0, 3)
lcd_print("Startup Complete!")

-- ついでにLEDをチカチカさせて起動を知らせる
for i=1, 3 do
    digitalWrite(13, 1)
    delay(100)
    digitalWrite(13, 0)
    delay(100)
end

