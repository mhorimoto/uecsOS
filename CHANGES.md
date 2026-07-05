# uecsOS パッチ変更サマリ

対象ブランチ: `main` / ディレクトリ: `luaTrial04/src/`
適用日: 2026-07-06

---

## 変更ファイル一覧

| ファイル | 変更種別 | 概要 |
|---|---|---|
| `USB_FT232H_MPSSE.h` | 変更 | FT_TimedRelay構造体・pinPulse()/processTimers()を追加 |
| `USB_FT232H_MPSSE.cpp` | 変更 | pinPulse()/processTimers()の実装を追加 |
| `lua_hw_usb.cpp` | 変更 | `usb.ft_pin_pulse()` Lua APIを追加（v0.0.6） |
| `lua_executor.cpp` | 変更 | lua_os_hook()内でft232h.processTimers()を呼ぶよう追加 |

※ `main.cpp` は **変更不要**
  loop()内の processTimers() 呼び出しは lua_os_hook() 側でカバーされるため。
  ただし、Luaスクリプトを実行していない通常のloop()でも呼びたい場合は
  以下を loop() に追記する：

```cpp
// main.cpp の loop() 末尾に追加（任意）
extern FT232H_MPSSE ft232h;
ft232h.processTimers();
```

---

## 新規API

### `usb.ft_pin_pulse(pin, duration_ms)`

ONして `duration_ms` ミリ秒後に自動OFFする非ブロッキング制御。

```lua
-- 例: d0を5秒間ONしてOFF（窓開けモーター5秒駆動）
usb.ft_pin_pulse("d0", 5000)

-- 例: c2を200msパルス
usb.ft_pin_pulse("c2", 200)
```

**動作仕様:**
- `delay()` を一切使用しない（ブロッキングゼロ）
- 同じピンに再度呼ばれた場合はタイマーをリセット（延長）
- タイマー処理は `loop()` と `lua_os_hook()` の両方で駆動
  → Lua無限ループ中でも10000命令ごとにOFF処理が走る
- millis()オーバーフロー対策済み（符号付き差分比較）
- 同時管理数: 最大16チャンネル（`FT_TIMED_RELAY_MAX`で変更可）

**農業制御の典型パターン:**

```lua
-- スクリプトを30秒ごとに呼び出す設計とし、
-- 温度条件が成立したときだけ pulse を発行する
local temp = uecs.get(1)
if temp > 28.0 then
    usb.ft_pin_pulse("d0", 5000)  -- 5秒だけ窓開けモーター駆動
end
-- → モーターが動き続けることはない（呼び出し間隔で自然に制御）
```

---

## 既存APIは変更なし

| API | 動作 |
|---|---|
| `usb.ft_pin(pin, state)` | 即時ON/OFF（タイマーなし）← 変更なし |
| `usb.ft_write_all(adbus, acbus)` | 全ピン一括制御 ← 変更なし |
| `usb.isConnected()` | 接続確認 ← 変更なし |
| `usb.begin()` | USBホスト起動 ← 変更なし |

---

## フェイルセーフ設計の補足（現場確認事項）

- リレー接点のNC/NO選択は季節・作物・栽培様式に依存するため現場確認必須
- 加温機など「止まると危険」な機器 → NC配線が安全側
- 灌水ポンプなど「動き続けると危険」な機器 → NO配線が安全側
- L1層（C++）での温度上限ガード機能も今後の検討事項
