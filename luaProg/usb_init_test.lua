print("--- uecsOS Hardware Initialization ---")

-- ==========================================================
-- 1. ハードウェアの論理マッピング定義（決め打ちの準備）
-- ※ "FTXXXXXX" はFT232H内蔵の固有シリアル。後日C++を拡張して取得します。
-- ==========================================================
local TARGET_PORTS = {
    PUMP_UNIT  = 3, -- ポンプ制御盤用のFT232H
    VALVE_UNIT = 2  -- 電磁弁制御盤用のFT232H
}

-- システム内で使用するデバイスID（0~4）の格納先
DEVICE = {
    PUMP_UNIT  = -1,
    VALVE_UNIT = -1
}

-- ==========================================================
-- 2. USBバスのリセットと初期化
-- ==========================================================
usb.power(false)
delay(500)
usb.begin()
delay(100)
usb.power(true)

print("Waiting for USB Enumeration...")
delay(2500) -- 全デバイスが認識されるまで十分な時間を待つ

-- ==========================================================
-- 3. デバイスの探索と割り当て
-- ==========================================================
local devs = usb.info()

for i, dev in ipairs(devs) do
    if dev.ready then
        -- 接続されているハブとポート番号を表示
        print(string.format("Slot[%d] Addr:%d Hub:%d Port:%d", dev.id, dev.address, dev.hub_addr, dev.hub_port))
        -- 物理ポート番号が一致した場合にデバイスIDを割り当て
        if dev.hub_port == TARGET_PORTS.PUMP_UNIT then
            DEVICE.PUMP_UNIT = dev.id
            print(" => Assigned PUMP_UNIT to Slot [" .. DEVICE.PUMP_UNIT .. "]")
        elseif dev.hub_port == TARGET_PORTS.VALVE_UNIT then
            DEVICE.VALVE_UNIT = dev.id
            print(" => Assigned VALVE_UNIT to Slot [" .. DEVICE.VALVE_UNIT .. "]")
        end
    end
end

-- ==========================================================
-- 4. 温室制御用 グローバル関数の定義
-- 他のスクリプト (exec1sec など) から簡単に呼べるように隠蔽化します
-- ==========================================================

function setPump1(state)
    if DEVICE.PUMP_UNIT >= 0 then
        -- ポンプ1は PUMP_UNIT の D0 ピンに接続されていると定義
        usb.ft_pin(DEVICE.PUMP_UNIT, "d0", state)
        print("  [Action] Pump 1 -> " .. tostring(state))
    else
        print("  [Error] PUMP_UNIT is offline!")
    end
end

function setValveA(state)
    if DEVICE.VALVE_UNIT >= 0 then
        -- 電磁弁Aは VALVE_UNIT の C3 ピンに接続されていると定義
        usb.ft_pin(DEVICE.VALVE_UNIT, "c3", state)
        print("  [Action] Valve A -> " .. tostring(state))
    else
        print("  [Error] VALVE_UNIT is offline!")
    end
end

-- ==========================================================
-- 5. ハードウェア動作テストシーケンス
-- ==========================================================
print("--- Running Hardware Test Sequence ---")

setPump1(true)
delay(1000)
setValveA(true)
delay(1000)

setPump1(false)
setValveA(false)

print("--- Hardware Init Completed ---")

