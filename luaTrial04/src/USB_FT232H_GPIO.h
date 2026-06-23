#ifndef USB_FT232H_GPIO_H
#define USB_FT232H_GPIO_H

#include <Arduino.h>
#include <USBHost_t36.h>

class USB_FT232H_GPIO : public USBDriver {
public:
    // コンストラクタで自前の init() を呼び出す
    USB_FT232H_GPIO(USBHost &host) { init(); }

    virtual bool claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len) override {
        if (type != 1) return false;

        // FT232Hの VID(0x0403) と PID(0x6014) を確認
        if (dev->idVendor != 0x0403 || dev->idProduct != 0x6014) {
            return false; 
        }

        Serial.println("FT232H GPIO Driver: FT232H Detected!");

        uint8_t rx_ep = 0, tx_ep = 0;
        uint16_t rx_size = 0, tx_size = 0;

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

        if (!rx_ep || !tx_ep) {
            Serial.println("FT232H GPIO Driver: Error - Bulk endpoints not found.");
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

    // --- 以降は今後の実装枠 ---
    bool setBitMode(uint8_t mask, uint8_t mode) { return false; }
    bool writeGPIO(uint8_t value) { return false; }
    int readGPIO() { return -1; }
    // --- 現在FT232Hが接続されているかを確認 ---
    bool isConnected() {
        return (device != nullptr);
    }

protected:
    Pipe_t *txpipe = nullptr;
    Pipe_t *rxpipe = nullptr;
    setup_t setupdata;
    uint8_t txbuf[64];
    uint8_t rxbuf[64];

private:
    // USBHost_t36 はドライバごとにメモリプールを静的に確保してOSに寄付する必要がある
    Pipe_t mypipes[2];
    Transfer_t mytransfers[4];

    // 自前で定義する初期化関数
    void init() {
        // OSのメモリプールへパイプと転送用メモリを寄付する
        contribute_Pipes(mypipes, sizeof(mypipes) / sizeof(Pipe_t));
        contribute_Transfers(mytransfers, sizeof(mytransfers) / sizeof(Transfer_t));
        // OSにこのドライバ自身を登録する
        driver_ready_for_device(this);
    }

    // 空のコールバック関数（リンクエラー回避）
    static void tx_callback(const Transfer_t *transfer) {}
    static void rx_callback(const Transfer_t *transfer) {}
};

#endif