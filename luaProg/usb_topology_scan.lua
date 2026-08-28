-- USBハブの物理ポート割り当て確認用スクリプト

print("=== USB Topology Scan ===")

-- C++側の MAX_FT232H_DEVICES (5) に合わせて0〜4までループ
for i = 0, 4 do
    -- ※ 'ft232h_get_topology' の部分は，uecsOSで実際に登録したCバインディングの関数名に合わせてください．
    local path = ft232h_get_topology(i) 

    if path == "Disconnected" or path == nil then
        print("Slot " .. i .. " : [ Disconnected ]")
    else
        print("Slot " .. i .. " : " .. path)
    end
end

print("=========================")