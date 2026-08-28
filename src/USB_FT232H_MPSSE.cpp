#include "USB_FT232H_MPSSE.h"

// 【追記】static配列の実体定義と初期化
uint8_t FT232H_MPSSE::topo_hub_addr[128] = {0};
uint8_t FT232H_MPSSE::topo_hub_port[128] = {0};

FT232H_MPSSE::FT232H_MPSSE(USBHost &host)
    : USBDriver(), bulk_in(nullptr), bulk_out(nullptr),
      device_ready(false), init_phase(0), claimed_dev(nullptr),
      current_adbus(0xFF), current_acbus(0xFF) {
    // タイマースロットを全クリア
    for (int i = 0; i < FT_TIMED_RELAY_MAX; i++) {
        _timers[i].active = false;
    }
    // トポロジ記録用マップの初期化
    for (int i = 0; i < 128; i++) {
        topo_hub_addr[i] = 0;
        topo_hub_port[i] = 0;
    }
    init();
}

void FT232H_MPSSE::init() {
    contribute_Pipes(mypipes, sizeof(mypipes) / sizeof(Pipe_t));
    contribute_Transfers(mytransfers, sizeof(mytransfers) / sizeof(Transfer_t));
    contribute_String_Buffers(mystring_bufs, sizeof(mystring_bufs) / sizeof(strbuf_t));
    driver_ready_for_device(this);
}

void FT232H_MPSSE::setADBUS(uint8_t value) {
    if (!device_ready) return;
    current_adbus = value;
    uint8_t cmd[3] = { 0x80, value, 0xFF };
    bulk_write(cmd, 3);
}

void FT232H_MPSSE::setACBUS(uint8_t value) {
    if (!device_ready) return;
    current_acbus = value;
    uint8_t cmd[3] = { 0x82, value, 0xFF };
    bulk_write(cmd, 3);
}

void FT232H_MPSSE::setAll(uint8_t adbus_val, uint8_t acbus_val) {
    if (!device_ready) return;
    current_adbus = adbus_val;
    current_acbus = acbus_val;
    uint8_t cmd[6] = {
        0x80, adbus_val, 0xFF,
        0x82, acbus_val, 0xFF
    };
    bulk_write(cmd, 6);
}

void FT232H_MPSSE::allOff() { setAll(0xFF, 0xFF); }
void FT232H_MPSSE::allOn()  { setAll(0x00, 0x00); }

// ============================================================
// 物理トポロジ・パスの取得
// ============================================================
String FT232H_MPSSE::getTopologyPath() {
    // ★修正1: 通信準備が完了していないインスタンス（誤認識スロット）は弾く
    if (!device_ready || claimed_dev == nullptr) {
        return "Disconnected";
    }

    String path = String(claimed_dev->hub_port);
    uint8_t current_parent_addr = claimed_dev->hub_address;
    int depth = 0; // 無限ループ防止

    while (current_parent_addr != 0 && depth < 10) {
        uint8_t p_addr = topo_hub_addr[current_parent_addr];
        uint8_t p_port = topo_hub_port[current_parent_addr];

        if (p_port == 0) {
            // ★修正2: USBHubドライバに先取りされてキャッシュが漏れた場合のフォールバック
            // 親ハブの「物理ポート」が不明な場合は「ハブのアドレス(ID)」で代用し、一意性を保つ
            path = "Hub(" + String(current_parent_addr) + ")-" + path;
            break;
        }

        path = String(p_port) + "-" + path;
        current_parent_addr = p_addr;
        depth++;
    }

    return "Root-" + path;
}


// ============================================================
// ビット単位の書き込み
// ============================================================
void FT232H_MPSSE::writeADBUSBit(uint8_t bit, bool state) {
    if (bit > 7) return;
    if (state) {
        current_adbus &= ~(1 << bit); // ON: ビットを0にする（負論理）
    } else {
        current_adbus |= (1 << bit);  // OFF: ビットを1にする（負論理）
    }
    setADBUS(current_adbus);
}

void FT232H_MPSSE::writeACBUSBit(uint8_t bit, bool state) {
    if (bit > 7) return;
    if (state) {
        current_acbus &= ~(1 << bit);
    } else {
        current_acbus |= (1 << bit);
    }
    setACBUS(current_acbus);
}

// ============================================================
// 非ブロッキング・パルス出力
//   ONして duration_ms 後に自動OFFする
//   同じピンに再度呼ばれた場合はタイマーをリセット（延長）する
//   ※農業制御では呼び出し間隔をLua側（スクリプト定期実行）で
//     制御するため、上書きリセットが自然な動作となる
// ============================================================
bool FT232H_MPSSE::pinPulse(char port, uint8_t bit, uint32_t duration_ms) {
    if (!device_ready) return false;

    // 即座にON
    if      (port == 'd') writeADBUSBit(bit, true);
    else if (port == 'c') writeACBUSBit(bit, true);
    else return false;

    // 既存スロットを検索（同じport/bitがあれば上書き）
    int slot = -1;
    for (int i = 0; i < FT_TIMED_RELAY_MAX; i++) {
        if (_timers[i].active && _timers[i].port == port && _timers[i].bit == bit) {
            slot = i;
            break;
        }
    }
    // なければ空きスロットを確保
    if (slot < 0) {
        for (int i = 0; i < FT_TIMED_RELAY_MAX; i++) {
            if (!_timers[i].active) { slot = i; break; }
        }
    }
    if (slot < 0) {
        Serial.println("[FT232H] pinPulse: timer slot full!");
        return false;
    }

    _timers[slot].active    = true;
    _timers[slot].port      = port;
    _timers[slot].bit       = bit;
    _timers[slot].off_at_ms = millis() + duration_ms;
    return true;
}

// ============================================================
// タイマー処理（loop() と lua_os_hook() の両方から呼ぶ）
//   millis() オーバーフロー対策として符号付き差分比較を使用
//   （約49日周期のオーバーフローでも正しく動作する）
// ============================================================
void FT232H_MPSSE::processTimers() {
    uint32_t now = millis();
    for (int i = 0; i < FT_TIMED_RELAY_MAX; i++) {
        if (!_timers[i].active) continue;
        if ((int32_t)(now - _timers[i].off_at_ms) >= 0) {
            // 期限切れ → OFFにする
            if (_timers[i].port == 'd') writeADBUSBit(_timers[i].bit, false);
            else                        writeACBUSBit(_timers[i].bit, false);
            _timers[i].active = false;
        }
    }
}

void FT232H_MPSSE::bulk_write(const uint8_t *data, uint32_t len) {
    if (!bulk_out || len > sizeof(write_buf)) return;
    memcpy(write_buf, data, len);
    queue_Data_Transfer(bulk_out, write_buf, len, this);
}

bool FT232H_MPSSE::claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len) {
    // 列挙された全デバイスの親ハブ情報を密かに記録しておく
    if (dev != nullptr && dev->address < 128) {
        topo_hub_addr[dev->address] = dev->hub_address;
        topo_hub_port[dev->address] = dev->hub_port;
    }

    // ★追加1: 自分がすでに他のデバイスを制御中(使用中)なら、この列挙処理を無視して他のインスタンスに譲る
    if (claimed_dev != nullptr) {
        return false;
    }

    if (type == 0) {
        if (dev->idVendor != 0x0403 || dev->idProduct != 0x6014) return false;
        // ★追加2: チャタリングによるログスパムを防ぐため、type=0 の段階ではログを出さずに静かに通過させる
        return false;
    }

    if (type == 1) {
        if (dev->idVendor != 0x0403 || dev->idProduct != 0x6014) return false;
        
        // 前回切断時のパイプ情報が残っていれば念のためクリア
        bulk_in = nullptr;
        bulk_out = nullptr;

        const uint8_t *p   = descriptors;
        const uint8_t *end = descriptors + len;
        while (p < end) {
            uint8_t desc_len  = p[0];
            uint8_t desc_type = p[1];
            if (desc_len == 0) break;
            if (desc_type == 5) {
                uint8_t  ep_addr = p[2];
                uint8_t  ep_type = p[3] & 0x03;
                uint16_t ep_size = p[4] | ((uint16_t)p[5] << 8);
                if (ep_type == 2) {
                    if (ep_addr & 0x80) {
                        bulk_in = new_Pipe(dev, 2, ep_addr & 0x0F, 1, ep_size);
                    } else {
                        bulk_out = new_Pipe(dev, 2, ep_addr & 0x0F, 0, ep_size);
                    }
                }
            }
            p += desc_len;
        }

        if (bulk_in && bulk_out) {
            claimed_dev = dev; // 通信準備が完全に整った「この瞬間」にだけデバイスを紐付ける
            
            // ★追加3: 接点が安定し、完全に接続を確立できたときだけログを出す
            Serial.print("[FT232H] Interface claimed. Starting MPSSE config... (");
            Serial.print(this->getTopologyPath());
            Serial.println(")");

            mk_setup(setup_pkt, 0x40, 0x00, 0x0000, 0x0000, 0);
            queue_Control_Transfer(dev, &setup_pkt, nullptr, this);
            init_phase = 0;
            return true;
        }
        return false;
    }
    return false;
}

void FT232H_MPSSE::control(const Transfer_t *transfer) {
    Device_t *dev = transfer->pipe->device;
    switch (init_phase) {
    case 0:
        mk_setup(setup_pkt, 0x40, 0x0B, 0x02FF, 0x0000, 0);
        queue_Control_Transfer(dev, &setup_pkt, nullptr, this);
        init_phase = 1;
        break;
    case 1:
        mk_setup(setup_pkt, 0x40, 0x09, 0x0010, 0x0000, 0);
        queue_Control_Transfer(dev, &setup_pkt, nullptr, this);
        init_phase = 2;
        break;
    case 2:
        Serial.println("[FT232H] MPSSE Init complete! Device ready.");
        device_ready = true;
        delayMicroseconds(10000); // 10ms
        allOff();
        break;
    }
}

void FT232H_MPSSE::disconnect() {
    Serial.println("[FT232H] Disconnected");
    device_ready = false;
    bulk_in     = nullptr;
    bulk_out    = nullptr;
    claimed_dev = nullptr;
    init_phase  = 0;
    current_adbus = 0xFF;
    current_acbus = 0xFF;
    // 切断時はタイマースロットも全クリア
    for (int i = 0; i < FT_TIMED_RELAY_MAX; i++) {
        _timers[i].active = false;
    }
}
