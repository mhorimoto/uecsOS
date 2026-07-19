# uecsOS パッチ3 変更サマリ（SCHEDコマンドによるスケジューラ切替）

対象: BLD 0.5.11（永続VM実装済み）→ 0.5.14
前提: uecsOS_patch2.zip（永続Lua VM実装）が反映済みであること

---

## 変更ファイル一覧

| ファイル | 変更種別 | 概要 |
|---|---|---|
| `lua_executor.h` | 変更 | `reload_persistent_lua()` / `get_active_scheduler_filename()` を追加 |
| `lua_executor.cpp` | 変更 | VM読み込み処理を`_persistent_vm_load()`に共通化、ポインタファイル対応 |
| `serial_shell.cpp` | 変更 | `SCHED` / `SCHED <filename>` コマンドを追加 |
| `main.cpp` | 変更なし | VERSIONのみ0.5.12に更新（patch2からロジック変更なし） |
| `scheduler.lua` | 変更なし | デフォルトのスケジューラプログラム（patch2と同一） |

---

## 新規コマンド：`SCHED`

シリアルシェルから使用します。

```
SCHED              → 現在アクティブなスケジューラファイル名を表示
SCHED progA.lua    → 永続VMを破棄・再生成し、progA.luaを即座に有効化
```

### 動作の流れ

```
SCHED progA.lua 実行
  ↓
1. 現在のg_lua_main（永続VM）をlua_close()で破棄
2. progA.lua が SDに存在するか確認
3. 新しいVMを生成し、progA.luaを読み込んで実行
   （トップレベルのグローバル変数初期化・exec関数定義が走る）
4. 成功したら /active_scheduler.txt に "progA.lua" と記録
5. 次回起動時は自動的にprogA.luaが読み込まれる
```

### 安全性について

FT232Hのパルスタイマー（`_timers[]`）はLua VMとは独立したC++オブジェクト
（`ft232h`）で管理されているため、`SCHED`でVMを切り替えても、
**動作中のパルスは中断されず、指定時間が来れば正しくOFFになります**。

---

## 複数プログラムの運用イメージ

```
SD:/
├── scheduler.lua        ← デフォルト（出荷時）
├── progA.lua             ← 農家が試作した制御パターンA
├── progB.lua             ← 制御パターンB
└── active_scheduler.txt  ← 現在選択中のファイル名（自動生成・自動更新）
```

農家やオペレーターは、シリアルシェル経由で自由に切り替えて試せます。
どのファイルが有効かは常に `SCHED`（引数なし）で確認できます。

---

## 既存コマンドとの違い（混同注意）

| コマンド | 対象 | 用途 |
|---|---|---|
| `LOAD <file>` / `RUN` | 対話編集バッファ（使い捨てVM） | デバッグ・単発コード実行 |
| `SCHED <file>` | **永続VM（スケジューラ）** | 本番の制御ロジック切り替え |

`LOAD`/`RUN`は行番号付きBASIC風のプログラム編集・使い捨て実行用で、
`exec1sec`等のスケジューラ関数とは無関係です。制御ロジックを切り替えたい
場合は必ず `SCHED` を使ってください。
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
