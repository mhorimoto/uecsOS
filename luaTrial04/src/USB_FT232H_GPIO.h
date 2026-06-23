#ifndef USB_FT232H_GPIO_H
#define USB_FT232H_GPIO_H

#include <Arduino.h>
#include <USBHost_t36.h>

class USB_FT232H_GPIO : public USBDriver {
public:
    USB_FT232H_GPIO(USBHost &host) { init(); }

    virtual bool claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len) override {
        // 【デバッグ】OSから何らかのUSBデバイスが提示されたら、その情報を全て表示する
        Serial.printf("USB claim() called -> type: %d, VID: %04X, PID: %04X\n", type, dev->idVendor, dev->idProduct);

        // FT232Hの VID(0x0403) と PID(0x6014) を確認
        if (dev->idVendor != 0x0403 || dev->idProduct != 0x6014) {
            return false; 
        }

        Serial.println("FT232H GPIO Driver: FT232H Target Found!");

        uint8_t rx_ep = 0, tx_ep = 0;
        uint16_t rx_size = 0, tx_size = 0;

        // ディスクリプタ（設計図）から送受信パイプを探す
        uint32_t index = 0;
        while (index < len) {
            uint8_t dlen = descriptors[index];
            uint8_t dtype = descriptors[index + 1];
            if (dlen < 2) break;

            if (dtype == 5) {
                uint8_t ep_addr = descriptors[index + 2];
                uint8_t ep_attr = descriptors[index + 3];
                uint16_t ep_size = descriptors[index + 4] | (descriptors[index + 5] << 8);

                if ((ep_attr & 0x03) == 2) { 
                    if (ep_addr & 0x80) { 
                        rx_ep = ep_addr;
                        rx_size = ep_size;
                    } else {              
                        tx_ep = ep_addr;
                        tx_size = ep_size;
                    }
                }
            }
            index += dlen;
        }

        // 見つからなかった場合はfalseを返して次の問い合わせ(type=0)を待つ
        if (!rx_ep || !tx_ep) {
            Serial.println("FT232H GPIO Driver: Endpoints not found in this descriptor (waiting for Interface level).");
            return false;
        }

        rxpipe = new_Pipe(dev, 2, rx_ep, 1, rx_size);
        txpipe = new_Pipe(dev, 2, tx_ep, 0, tx_size);

        if (!rxpipe || !txpipe) {
            Serial.println("FT232H GPIO Driver: Error - Failed to allocate pipes.");
            return false;
        }

        rxpipe->callback_function = rx_callback;
        txpipe->callback_function = tx_callback;

        device = dev; 
        Serial.println("FT232H GPIO Driver: Claimed successfully! Ready for Bitbang.");
        
        return true;
    }

    virtual void disconnect() override {
        if (rxpipe) rxpipe = nullptr; 
        if (txpipe) txpipe = nullptr;
        device = nullptr;
        Serial.println("FT232H GPIO Driver: Disconnected.");
    }

    virtual void control(const Transfer_t *transfer) override {}

    // ---------------------------------------------------------
    // FT232H 固有のハードウェア制御 API
    // ---------------------------------------------------------

    // Bitbang（GPIO）モードへの移行コマンドを送信する
    bool setBitMode(uint8_t mask, uint8_t mode) {
        if (!device) return false;

        // 【修正】USBコントロール転送用パケット(setup_t)を手動で組み立てる
        setupdata.bmRequestType = 0x40;               // Vendor, Host-to-Device
        setupdata.bRequest = 0x0B;                    // SIO_SET_BITMODE
        setupdata.wValue = (mode << 8) | mask;        // 上位8bitがモード、下位8bitがマスク
        setupdata.wIndex = 1;                         // Interface 1
        setupdata.wLength = 0;                        // データフェーズなし(送信のみ)

        // 【修正】引数は4つ: デバイス, setup構造体ポインタ, データバッファ(今回は無し), ドライバ(自分自身)
        return queue_Control_Transfer(device, &setupdata, NULL, this);
    }

    // GPIOの出力状態を変更する（Bulk OUTパイプへの1バイト送信）
    bool writeGPIO(uint8_t value) {
        if (!device || !txpipe) return false;
        txbuf[0] = value;
        return queue_Data_Transfer(txpipe, txbuf, 1, this);
    }

    int readGPIO() { return -1; }

    // --- 接続状態の確認 ---
    bool isConnected() { return (device != nullptr); }

protected:
    Pipe_t *txpipe = nullptr;
    Pipe_t *rxpipe = nullptr;
    setup_t setupdata;
    uint8_t txbuf[64];
    uint8_t rxbuf[64];

private:
    Pipe_t mypipes[2];
    Transfer_t mytransfers[4];

    void init() {
        contribute_Pipes(mypipes, sizeof(mypipes) / sizeof(Pipe_t));
        contribute_Transfers(mytransfers, sizeof(mytransfers) / sizeof(Transfer_t));
        driver_ready_for_device(this);
    }

    static void tx_callback(const Transfer_t *transfer) {}
    static void rx_callback(const Transfer_t *transfer) {}
};

#endif