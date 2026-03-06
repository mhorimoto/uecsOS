-- chkusb.lua
-- MPSSE初期化関数
function mpsse_init()
    print("Initializing FT232H MPSSE mode...")

    -- 1. USBホストの開始 (12MHzなど適切な速度で)
    if not usb_begin(12000000) then
        print("Error: Failed to start USB Host.")
        return false
    end
    delay(100)

    -- 2. 同期確認 (Bad Command Test)
    -- 存在しないコマンド 0xAA を送り、0xFA 0xAA が返るか確認
    usb_write("\xAA")
    delay(50)
    local res = usb_read()
    
    -- バイナリ比較のため string.byte 等を使うのが確実ですが、簡易的にチェック
    if res:find("\xFA\xAA") then
        print("MPSSE Synchronization: OK")
    else
        print("MPSSE Synchronization: Failed. Response: " .. (res or "nil"))
        -- 同期失敗時はリトライするか終了
    end

    -- 3. クロック設定
    -- 0x8A: Disable clock divide by 5 (60MHz master clock)
    -- 0x97: Adaptive clocking disable (通常は不要)
    usb_write("\x8A\x97")
    
    -- 4. 通信速度（ディバイダ）の設定
    -- 0x86: Set TCK/SK Divisor
    -- 続く2バイトで分周比を指定 (Value = 60MHz / ((1 + Divisor) * 2))
    -- 例: 0x0005 -> 約5MHz
    usb_write("\x86\x05\x00")

    -- 5. GPIO方向設定 (下位ビット ADBUS)
    -- 0x80: Set Data Bits Low Byte
    -- 第2引数: 出力値 (0x00 = All Low)
    -- 第3引数: 方向 (0xFF = All Output)
    -- 農業用リレーなどを繋ぐ場合はここで方向を定義します
    usb_write("\x80\x00\xFF")
    
    print("MPSSE setup completed.")
    return true
end

-- 実行
if mpsse_init() then
    lcd_clear()
    lcd_setCursor(0, 0)
    lcd_print("USB MPSSE Ready")
else
    lcd_setCursor(0, 0)
    lcd_print("USB Init Error")
end

