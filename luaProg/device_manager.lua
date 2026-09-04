-- /device_manager.lua : トポロジ動的解決および高水準制御API

DEV_SLOTS = {}

-- 1. トポロジの自動スキャンと論理名バインド
function init_devices()
    DEV_SLOTS = {}
    local dev_list = usb.info()
    print("=== USB Topology Binding ===")

    for _, d in ipairs(dev_list) do
        if d.ready then
            local conf = DEVICE_CONFIG[d.path]
            if conf then
                DEV_SLOTS[conf.name] = d.id
                print(string.format("[Map] %-12s (Slot %d) <= %s", conf.name, d.id, d.path))
            else
                print(string.format("[Warn] Unregistered device at %s (Slot %d)", d.path, d.id))
            end
        end
    end
    print("============================")
end

-- 2. 論理名指定によるピン即時制御（MAKE / BREAK）
function relay_set(dev_name, pin, state)
    local slot = DEV_SLOTS[dev_name]
    if slot ~= nil then
        return usb.ft_pin(slot, pin, state)
    else
        print("[Error] Device not bound: " .. tostring(dev_name))
        return false
    end
end

-- 3. 論理名指定による非ブロッキングパルス制御
function relay_pulse(dev_name, pin, duration_ms)
    local slot = DEV_SLOTS[dev_name]
    if slot ~= nil then
        return usb.ft_pulse(slot, pin, duration_ms)
    else
        print("[Error] Device not bound: " .. tostring(dev_name))
        return false
    end
end