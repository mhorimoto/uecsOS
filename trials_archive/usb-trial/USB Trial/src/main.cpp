#include <Arduino.h>
#include <USBHost_t36.h>

USBHost myusb;
USBHub hub1(myusb);

// ============================================================
// FT232H MPSSEドライバ
// ============================================================
class FT232H_MPSSE : public USBDriver {
public:
    FT232H_MPSSE(USBHost &host)
        : USBDriver(), bulk_in(nullptr), bulk_out(nullptr),
          device_ready(false), init_phase(0), claimed_dev(nullptr) {
        init();
    }

    bool isReady() { return device_ready; }

    void setADBUS(uint8_t value) {
        if (!device_ready) return;
        uint8_t cmd[3] = { 0x80, value, 0xFF };
        bulk_write(cmd, 3);
    }

    void setACBUS(uint8_t value) {
        if (!device_ready) return;
        uint8_t cmd[3] = { 0x82, value, 0xFF };
        bulk_write(cmd, 3);
    }

    void setAll(uint8_t adbus_val, uint8_t acbus_val) {
        if (!device_ready) return;
        uint8_t cmd[6] = {
            0x80, adbus_val, 0xFF,
            0x82, acbus_val, 0xFF
        };
        bulk_write(cmd, 6);
    }

    void allOff() { setAll(0xFF, 0xFF); }
    void allOn()  { setAll(0x00, 0x00); }

protected:
    virtual bool claim(Device_t *dev, int type,
                       const uint8_t *descriptors, uint32_t len) override {
        if (type == 0) {
            if (dev->idVendor != 0x0403 || dev->idProduct != 0x6014) return false;
            Serial.println("[FT232H] Device matched (type=0), waiting for interface...");
            claimed_dev = dev;
            return false;
        }

        if (type == 1) {
            if (dev->idVendor != 0x0403 || dev->idProduct != 0x6014) return false;
            Serial.println("[FT232H] Interface claim (type=1)");

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
                mk_setup(setup_pkt, 0x40, 0x00, 0x0000, 0x0000, 0);
                queue_Control_Transfer(dev, &setup_pkt, nullptr, this);
                init_phase = 0;
                return true;
            }
            return false;
        }
        return false;
    }

    virtual void control(const Transfer_t *transfer) override {
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
                delay(10);
                allOff();
                break;
        }
    }

    virtual void disconnect() override {
        Serial.println("[FT232H] Disconnected");
        device_ready = false;
        bulk_in     = nullptr;
        bulk_out    = nullptr;
        claimed_dev = nullptr;
        init_phase  = 0;
    }

private:
    Pipe_t     mypipes[5]      __attribute__((aligned(32)));
    Transfer_t mytransfers[24] __attribute__((aligned(32)));
    strbuf_t   mystring_bufs[2];
    setup_t    setup_pkt;
    Pipe_t    *bulk_in;
    Pipe_t    *bulk_out;
    bool       device_ready;
    int        init_phase;
    Device_t  *claimed_dev;
    uint8_t    write_buf[16]   __attribute__((aligned(32)));

    void init() {
        contribute_Pipes(mypipes, sizeof(mypipes) / sizeof(Pipe_t));
        contribute_Transfers(mytransfers, sizeof(mytransfers) / sizeof(Transfer_t));
        contribute_String_Buffers(mystring_bufs, sizeof(mystring_bufs) / sizeof(strbuf_t));
        driver_ready_for_device(this);
    }

    void bulk_write(const uint8_t *data, uint32_t len) {
        if (!bulk_out || len > sizeof(write_buf)) return;
        memcpy(write_buf, data, len);
        queue_Data_Transfer(bulk_out, write_buf, len, this);
    }
};

// ============================================================
// グローバルインスタンス & LED状態
// ============================================================
FT232H_MPSSE ft232h(myusb);

// 負論理：0=点灯, 1=消灯。初期値は全消灯(0xFF)
uint8_t adbus_state = 0xFF;
uint8_t acbus_state = 0xFF;

// ============================================================
// LEDトグル処理
// ============================================================

// ピン名のパース: "d0"〜"d7" → ADBUS0〜7、"c0"〜"c7" → ACBUS0〜7
// 戻り値: true=成功, false=無効入力
bool parsePin(const String &input, char &bus, uint8_t &bit) {
    if (input.length() < 2) return false;

    char b = tolower(input.charAt(0));
    if (b != 'c' && b != 'd') return false;

    int n = input.substring(1).toInt();
    if (n < 0 || n > 7) return false;
    // 数字以外が来た場合の弾き（例: "dx"）
    for (int i = 1; i < (int)input.length(); i++) {
        if (!isDigit(input.charAt(i))) return false;
    }

    bus = b;
    bit = (uint8_t)n;
    return true;
}

void toggleLED(char bus, uint8_t bit) {
    if (bus == 'd') {
        // ADUSのbitをトグル
        adbus_state ^= (1 << bit);
        ft232h.setADBUS(adbus_state);
        bool on = !(adbus_state & (1 << bit)); // 負論理なので反転
        Serial.printf("  ADBUS%d: %s (ADBUS=0x%02X)\n", bit, on ? "ON" : "OFF", adbus_state);
    } else {
        // ACBUSのbitをトグル
        acbus_state ^= (1 << bit);
        ft232h.setACBUS(acbus_state);
        bool on = !(acbus_state & (1 << bit));
        Serial.printf("  ACBUS%d: %s (ACBUS=0x%02X)\n", bit, on ? "ON" : "OFF", acbus_state);
    }
}

// 現在の点灯状態を一覧表示
void printStatus() {
    Serial.println("\n--- LED Status ---");
    Serial.print("  ADBUS: ");
    for (int i = 7; i >= 0; i--) {
        Serial.print(!(adbus_state & (1 << i)) ? "1" : "0"); // 負論理反転表示
    }
    Serial.println(" (bit7..bit0, 1=ON)");

    Serial.print("  ACBUS: ");
    for (int i = 7; i >= 0; i--) {
        Serial.print(!(acbus_state & (1 << i)) ? "1" : "0");
    }
    Serial.println(" (bit7..bit0, 1=ON)");

    // 点灯中のピンを列挙
    Serial.print("  ON: ");
    bool any = false;
    for (int i = 0; i < 8; i++) {
        if (!(adbus_state & (1 << i))) { Serial.printf("d%d ", i); any = true; }
    }
    for (int i = 0; i < 8; i++) {
        if (!(acbus_state & (1 << i))) { Serial.printf("c%d ", i); any = true; }
    }
    if (!any) Serial.print("(none)");
    Serial.println("\n------------------");
}

void printHelp() {
    Serial.println("\n=== Commands ===");
    Serial.println("  d0 ~ d7  : ADBUS0〜7 トグル");
    Serial.println("  c0 ~ c7  : ACBUS0〜7 トグル");
    Serial.println("  all on   : 全LED点灯");
    Serial.println("  all off  : 全LED消灯");
    Serial.println("  status   : 点灯状態表示");
    Serial.println("  help     : このヘルプ");
    Serial.println("================\n");
}

// ============================================================
// Setup / Loop
// ============================================================
void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 4000);
    Serial.println("\n--- Teensy 4.1 FT232H MPSSE LED Toggle ---");

    IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_40 = 5;
    IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_40 = 0x0008;
    GPIO8_GDIR     |= (1 << 26);
    GPIO8_DR_CLEAR  = (1 << 26);
    Serial.println("1. 5V Power OFF");
    delay(500);

    myusb.begin();
    Serial.println("2. USB Host Core Started");
    delay(200);

    GPIO8_DR_SET = (1 << 26);
    Serial.println("3. 5V Power ON - Please connect FT232H");
}

String inputBuf = "";

void loop() {
    myusb.Task();

    // デバイス準備完了の通知（1回だけ）
    static bool welcomed = false;
    if (ft232h.isReady() && !welcomed) {
        welcomed = true;
        printHelp();
        printStatus();
    }
    if (!ft232h.isReady()) {
        welcomed = false;
    }

    // シリアル入力処理
    while (Serial.available()) {
        char c = Serial.read();

        if (c == '\r') continue; // CR無視

        if (c == '\n') {
            // 入力確定
            inputBuf.trim();
            inputBuf.toLowerCase();

            if (inputBuf.length() == 0) {
                inputBuf = "";
                continue;
            }

            Serial.print("> ");
            Serial.println(inputBuf);

            if (!ft232h.isReady()) {
                Serial.println("  [ERROR] FT232H not connected.");
            } else if (inputBuf == "all on") {
                adbus_state = 0x00;
                acbus_state = 0x00;
                ft232h.allOn();
                Serial.println("  All LEDs ON");
                printStatus();
            } else if (inputBuf == "all off") {
                adbus_state = 0xFF;
                acbus_state = 0xFF;
                ft232h.allOff();
                Serial.println("  All LEDs OFF");
                printStatus();
            } else if (inputBuf == "status") {
                printStatus();
            } else if (inputBuf == "help") {
                printHelp();
            } else {
                char bus;
                uint8_t bit;
                if (parsePin(inputBuf, bus, bit)) {
                    toggleLED(bus, bit);
                } else {
                    Serial.printf("  [ERROR] Unknown command: '%s'\n", inputBuf.c_str());
                    Serial.println("  Type 'help' for commands.");
                }
            }

            inputBuf = "";
        } else {
            inputBuf += c;
        }
    }
}
