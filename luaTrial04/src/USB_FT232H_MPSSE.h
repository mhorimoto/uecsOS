#ifndef USB_FT232H_MPSSE_H
#define USB_FT232H_MPSSE_H

#include <Arduino.h>
#include <USBHost_t36.h>

class FT232H_MPSSE : public USBDriver {
public:
    FT232H_MPSSE(USBHost &host);

    bool isReady() { return device_ready; }

    // バイト単位の直接書き込み
    void setADBUS(uint8_t value);
    void setACBUS(uint8_t value);
    void setAll(uint8_t adbus_val, uint8_t acbus_val);
    void allOff();
    void allOn();

    // 【新規追加】ビット単位の書き込み（state: true=ON/点灯, false=OFF/消灯）
    // ※内部で負論理(0=ON, 1=OFF)への変換を自動で行います
    void writeADBUSBit(uint8_t bit, bool state);
    void writeACBUSBit(uint8_t bit, bool state);

protected:
    virtual bool claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len) override;
    virtual void control(const Transfer_t *transfer) override;
    virtual void disconnect() override;

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

    // ピンの状態をクラス内で保持（初期値は全消灯 = 0xFF）
    uint8_t    current_adbus;
    uint8_t    current_acbus;

    void init();
    void bulk_write(const uint8_t *data, uint32_t len);
};

#endif // USB_FT232H_MPSSE_H