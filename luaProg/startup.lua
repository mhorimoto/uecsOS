-- /startup.lua : uecsOS 起動時初期化スクリプト

lcd.setCursor(0, 1)
lcd.print("USB Initializing.")
print("USB Initializing...")

-- 1. USBホストコアの起動と5Vバスパワー給電開始
usb.begin()
usb.power(true)

lcd.setCursor(0, 1)
lcd.print("USB Powering up. ")
print("USB Powering up... Waiting for bus enumeration...")

-- 2. USBデバイス・ハブの列挙完了を待機 (スマート待機で約1.5秒)
delay(1500)

-- 3. 設定ファイルおよびデバイスマネージャのロード
print("Loading configurations...")
dofile("config.lua")
dofile("device_manager.lua")

-- 4. 物理トポロジの自動解決と論理名バインド
init_devices()

lcd.setCursor(0, 1)
lcd.print("USB Initialized. ")
print("=== System Startup Sequence Completed ===")