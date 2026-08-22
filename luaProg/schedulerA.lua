-- ============================================================
-- scheduler.lua
-- Loaded only once on the persistent Lua VM (once at startup)
-- Global variables and functions defined here
-- persist across calls to exec1sec, exec10sec, and exec1min
--
-- ============================================================
-- ---- Initialization of global state (executed only once at startup) ----
pins = {
    "c0","c1","c2","c3","c4","c5","c6","c7",  -- ACBUS 0-7
    "d0","d1","d2","d3","d4","d5","d6","d7"   -- ADBUS 0-7
}
pin_idx = 1     -- Channel to pulse next (Lua is 1-indexed)
c = 0           -- exec10sec call counter
print("[scheduler] Initialized. pins=" .. #pins)
-- ============================================================
-- Called every second (triggered by the UECS standard 1-second interval on the C++ side)
-- ============================================================
function exec1sec()
  -- Currently unused. Can be used for functions such as the LCD heartbeat display.
end
-- ============================================================
-- Called every 10 seconds
--   Only once every 4 calls (i.e., once every 40 seconds), pulse the next channel for 10 seconds
-- ============================================================
function exec10sec()
    c = c + 1
    if c > 3 then
        c = 0
        if not usb.isConnected() then
            print("[exec10sec] FT232H not ready, skip")
            return
        end
        local pin = pins[pin_idx]
        print("[exec10sec] Pulse " .. pin .. " for 10000ms")
        usb.ft_pin_pulse(pin, 10000)
        pin_idx = pin_idx + 1
        if pin_idx > #pins then
            pin_idx = 1
        end
    end
end
-- ============================================================
-- Called every minute
-- ============================================================
function exec1min()
-- Currently unused. Can be used for long-cycle monitoring, log output, etc.
end
