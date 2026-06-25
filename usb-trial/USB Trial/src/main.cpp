#include <Arduino.h>
#include <USBHost_t36.h>

USBHost myusb;
USBHub hub1(myusb);

// デバッグ用のダミードライバ
class USBDebugger : public USBDriver {
public:
    USBDebugger(USBHost &host) { init(); }
    virtual bool claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len) override {
        Serial.printf("\n>>> USB Device Detected! type: %d, VID: %04X, PID: %04X <<<\n", type, dev->idVendor, dev->idProduct);
        return false; 
    }
    virtual void disconnect() override {
        Serial.println("\n>>> USB Device Disconnected! <<<");
    }
    virtual void control(const Transfer_t *transfer) override {}
private:
    Pipe_t mypipes[3];
    Transfer_t mytransfers[12]; // FT232Hの長い設計図を読み取れるよう12へ増強
    void init() {
        contribute_Pipes(mypipes, 3);
        contribute_Transfers(mytransfers, 12);
        driver_ready_for_device(this);
    }
};

USBDebugger usb_debug(myusb);

void setup() {
Serial.begin(115200);
    while (!Serial && millis() < 4000); 
    Serial.println("\n--- Teensy 4.1 USB Host Hardware Test ---");

    // 【改善1】まず最初に5V電源を「明示的にOFF」にする（物理的に抜いた状態を作る）
    IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_40 = 5;
    IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_40 = 0x0008;
    GPIO8_GDIR |= (1 << 26);
    GPIO8_DR_CLEAR = (1 << 26); // ★CLEARでOFF
    Serial.println("1. 5V Power OFF (Resetting USB bus...)");

    // デバイスの残留電力が完全に抜けるまで待つ
    delay(500); 

    // 【改善2】電源が落ちている安全な状態で、USBホストコアを起動する
    myusb.begin();
    delay(10);
    USBHS_PORTSC1 |= (1 << 24);
    Serial.println("2. USB Host Core Started (Forced 12Mbps).");

    // ホストコアの初期化完了を少し待つ
    delay(100);

    // 【改善3】ホスト側の受け入れ準備が整ってから、5V電源を「ON」にする
    GPIO8_DR_SET = (1 << 26); // ★SETでON
    Serial.println("3. 5V Power ON (Simulating Hot-Plug)");
}

void loop() {
    myusb.Task(); 
}