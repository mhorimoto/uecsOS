#ifndef USB_FT232H_MPSSE_H
#define USB_FT232H_MPSSE_H

#include <Arduino.h>
#include <USBHost_t36.h>

// ============================================================
// FT232Hのシステム全体での最大接続台数
// ============================================================
#define MAX_FT232H_DEVICES 5
// ============================================================
// 非ブロッキング・タイマー管理（ft_pin_pulse用）
// ============================================================
#define FT_TIMED_RELAY_MAX 16   // 同時管理できるチャンネル数上限

struct FT_TimedRelay {
    bool     active;       // このスロットが使用中か
    char     port;         // 'd'=ADBUS, 'c'=ACBUS
    uint8_t  bit;          // ビット番号 0-7
    uint32_t off_at_ms;    // この millis() を過ぎたらOFFにする
};

class FT232H_MPSSE : public USBDriver {
public:
    FT232H_MPSSE(USBHost &host);

    bool isReady() { return device_ready; }
    // USB_FT232H_MPSSE.h の public: セクションに追記
    uint16_t getVID() { return (claimed_dev != nullptr) ? claimed_dev->idVendor : 0; }
    uint16_t getPID() { return (claimed_dev != nullptr) ? claimed_dev->idProduct : 0; }
    String getSerial() {
        const uint8_t *sn = serialNumber();
        return (sn != nullptr && sn[0] != '\0') ? String((const char*)sn) : String("EMPTY");
    }
    String getManufacturer() {
        const uint8_t *mfg = manufacturer();
        return (mfg != nullptr && mfg[0] != '\0') ? String((const char*)mfg) : String("EMPTY");
    }
    String getProduct() {
        const uint8_t *prd = product();
        return (prd != nullptr && prd[0] != '\0') ? String((const char*)prd) : String("EMPTY");
    }
    // 物理接続トポロジ（ポート位置）の取得
    uint8_t getAddress() { return (claimed_dev != nullptr) ? claimed_dev->address : 0; }
    uint8_t getHubAddress() { return (claimed_dev != nullptr) ? claimed_dev->hub_address : 0; }
    uint8_t getHubPort() { return (claimed_dev != nullptr) ? claimed_dev->hub_port : 0; }

    // バイト単位の直接書き込み
    void setADBUS(uint8_t value);
    void setACBUS(uint8_t value);
    void setAll(uint8_t adbus_val, uint8_t acbus_val);
    void allOff();
    void allOn();

    // ビット単位の書き込み（state: true=ON, false=OFF）
    // 内部で負論理(0=ON, 1=OFF)への変換を自動で行う
    void writeADBUSBit(uint8_t bit, bool state);
    void writeACBUSBit(uint8_t bit, bool state);

    // ============================================================
    // 非ブロッキング・パルス出力
    //   ONして duration_ms 後に自動OFFする（delay()不使用）
    //   loop() および lua_os_hook() から processTimers() を呼ぶこと
    // ============================================================
    bool pinPulse(char port, uint8_t bit, uint32_t duration_ms);
    void processTimers();   // loop() と lua_os_hook() の両方から呼ぶ

protected:
    virtual bool claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len) override;
    virtual void control(const Transfer_t *transfer) override;
    virtual void disconnect() override;

private:
    Pipe_t     mypipes[5]      __attribute__((aligned(32)));
    Transfer_t mytransfers[24] __attribute__((aligned(32)));
    strbuf_t   mystring_bufs[3];
    setup_t    setup_pkt;
    Pipe_t    *bulk_in;
    Pipe_t    *bulk_out;
    bool       device_ready;
    int        init_phase;
    Device_t  *claimed_dev;
    uint8_t    write_buf[16]   __attribute__((aligned(32)));

    // ピンの状態をクラス内で保持（初期値は全消灯 = 0xFF）
    uint8_t    current_adbus;
    uint8_t    current_acbus;

    // タイマースロット
    FT_TimedRelay _timers[FT_TIMED_RELAY_MAX];

    void init();
    void bulk_write(const uint8_t *data, uint32_t len);
};
// ============================================================
// グローバル・インスタンス配列の外部参照宣言
// 各cppファイルでこのヘッダをインクルードするだけで ft_devices にアクセス可能になる
// ============================================================
extern FT232H_MPSSE* ft_devices[MAX_FT232H_DEVICES];
extern USBHost myusb;

#endif // USB_FT232H_MPSSE_H
