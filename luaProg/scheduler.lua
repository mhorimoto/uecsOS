-- /scheduler.lua : uecsOS 自律分散環境制御スケジューラ

print("[Scheduler] Initializing persistent environment...")

-- 1. 物理トポロジ設定とデバイスマネージャのロード
dofile("config.lua")
dofile("device_manager.lua")

-- 2. トポロジの解決と論理名バインド
init_devices()

-- 3. UECS受信スロットの登録 (外部からの気温データを10秒周期で監視)
-- Room=1, Region=1, Order=1, Priority=15, Lifespan=10秒 (30秒未達でexpired)
uecs.publish("R", "InAirTemp", 1, 1, 1, 15, 0.0, 1, 1, 10)

-- 内部カウンタ
local counter_1sec = 0
local counter_10sec = 0

-- 目標温度設定
local TARGET_TEMP = 25.0

-- ============================================================
-- 1秒周期コールバック
-- ============================================================
function exec1sec()
    counter_1sec = counter_1sec + 1
    -- ハートビート点滅等の用途（今回は静かに維持）
end

-- ============================================================
-- 10秒周期コールバック (環境計測値に基づく自律制御ループ)
-- ============================================================
function exec10sec()
    counter_10sec = counter_10sec + 1
    
    -- UECSコアから最新の InAirTemp を取得
    local temp_val, is_valid = uecs.get("InAirTemp", 1, 1)

    if is_valid then
        print(string.format("[Control:10s #%d] InAirTemp = %.1f C (Target: %.1f C)", 
                            counter_10sec, temp_val, TARGET_TEMP))
        
        if temp_val >= TARGET_TEMP then
            -- 温度が目標値を超えていれば、換気ファン系統(RELAY_BOX_A d0)を3秒間パルス駆動
            print("  -> Temp HIGH! Activating Ventilation (RELAY_BOX_A d0 3000ms)")
            relay_pulse("RELAY_BOX_A", "d0", 3000)
        else
            print("  -> Temp OK. Ventilation idle.")
        end
    else
        -- 外部センサパケットが途切れている場合（フェイルセーフ）
        print(string.format("[Control:10s #%d] InAirTemp is INVALID or TIMEOUT. Holding relays safe.", 
                            counter_10sec))
    end
end

-- ============================================================
-- 1分周期コールバック
-- ============================================================
function exec1min()
    print("[Scheduler:1min] Periodic report:")
    print("  Uptime: " .. tostring(uecs.uptime()) .. " sec")
    print("  Lua Memory: " .. tostring(luamem()) .. " bytes")
end

print("[Scheduler] Initialization completed successfully.")
