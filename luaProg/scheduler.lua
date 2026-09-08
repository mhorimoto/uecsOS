-- /scheduler.lua : uecsOS 自律分散スケジューラ定義スクリプト

print("[Scheduler] Initializing persistent environment...")

-- 1. 物理トポロジ設定とデバイスマネージャのロード
dofile("config.lua")
dofile("device_manager.lua")

-- 2. トポロジの解決と論理名バインド
init_devices()

-- 内部状態カウンタ
local counter_1sec = 0
local counter_10sec = 0

-- ============================================================
-- 1秒周期コールバック (C++ main.cpp: exec1sec から非同期呼び出し)
-- ============================================================
function exec1sec()
    counter_1sec = counter_1sec + 1
    
    -- 例: 5秒に1回、RELAY_BOX_AのLED(d0)を500msパルス点滅
    if counter_1sec % 5 == 0 then
        print(string.format("[Scheduler:1s #%d] Pulsing RELAY_BOX_A d0 for 500ms", counter_1sec))
        -- 非ブロッキングパルス出力 (500ms)
        relay_pulse("RELAY_BOX_A", "d0", 500)
    end
end

-- ============================================================
-- 10秒周期コールバック (C++ main.cpp: exec10sec から非同期呼び出し)
-- ============================================================
function exec10sec()
    counter_10sec = counter_10sec + 1
    print(string.format("[Scheduler:10s #%d] Running autonomous environmental loop", counter_10sec))

    -- 例: カスケード先の RELAY_BOX_B の d7 (バルブ系統等) を2秒パルス駆動
    relay_pulse("RELAY_BOX_B", "d7", 2000)
end

-- ============================================================
-- 1分周期コールバック (C++ main.cpp: exec1min から非同期呼び出し)
-- ============================================================
function exec1min()
    print("[Scheduler:1min] Periodic report:")
    print("  Uptime: " .. tostring(uecs.uptime()) .. " sec")
    print("  Lua Memory: " .. tostring(luamem()) .. " bytes")
end

print("[Scheduler] Initialization completed successfully.")
