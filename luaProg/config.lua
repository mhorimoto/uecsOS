-- /config.lua : 物理トポロジと論理デバイスの定義テーブル
DEVICE_CONFIG = {
    ["Root-Hub(1)-1"] = {
        name = "RELAY_BOX_A",
        desc = "換気・ファン系統"
    },
    ["Root-Hub(1)-4-1"] = {
        name = "RELAY_BOX_B",
        desc = "灌水・電磁弁系統"
    }
}
return {
    show_ip = true,
    show_status = true,
    show_version = true
}
