/*
 * ==============================================================================
 * Teensy 4.1 - ILI9488 8-bit Parallel (8080-II / DBI Type B) 最小疎通テスト
 * ==============================================================================
 * 
 * 【背景・目的】
 * Teensy 4.1 (600MHz) による高速GPIOトグルに対し、ILI9488の8080バス仕様
 * （tWRL/tWRH 最小15〜30ns、配線容量やリンギングを考慮すると50〜200ns推奨）
 * に適合した十分なパルス幅・セットアップ時間を確保し、確実にコマンド・データを
 * ラッチさせる最小疎通スケッチです。
 * 
 * 【結線定義】（ili9488_lcd.h / 各種PIN接続 スプレッドシート準拠）
 *   LCD_CS    : Pin 10
 *   LCD_WR    : Pin 14
 *   LCD_DC/RS : Pin 15
 *   LCD_RST   : Pin 9  (明示的にリセットシーケンスを実行)
 * 
 *   DB0 : Pin 28
 *   DB1 : Pin 29
 *   DB2 : Pin 30
 *   DB3 : Pin 31
 *   DB4 : Pin 35  (※スプレッドシート表記34とili9488_lcd.h表記35の差異に注意)
 *   DB5 : Pin 34  (※同上)
 *   DB6 : Pin 40
 *   DB7 : Pin 41
 * 
 *   電源 : 3.3V (VCC / VDD), 5V (LED_A / バックライト), GND
 * ==============================================================================
 */

#include <Arduino.h>

// ------------------------------------------------------------------------------
// ピン定義
// ------------------------------------------------------------------------------
#define TFT_CS   10
#define TFT_WR   14
#define TFT_DC   15   // RS (Register / Data Select)
#define TFT_RST  9    // ハードウェアリセット

// 8ビットデータバス
#define TFT_D0   28
#define TFT_D1   29
#define TFT_D2   30
#define TFT_D3   31
#define TFT_D4   35   // ili9488_lcd.h の定義に準拠（入れ替え時は34に変更）
#define TFT_D5   34   // ili9488_lcd.h の定義に準拠（入れ替え時は35に変更）
#define TFT_D6   40
#define TFT_D7   41

// ------------------------------------------------------------------------------
// タイミング調整パラメータ
// Teensy 4.1 (600MHz: 1クロック = 約1.67ns)
// ブレッドボード配線やジャンパ線による浮遊容量・反射を考慮し、
// 初回疎通時は確実に認識される 200ns（または 500ns）からテストします。
// ------------------------------------------------------------------------------
#define WR_PULSE_DELAY_NS  200  // WRパルス幅 (LOW時間 / HIGH時間: ナノ秒単位)
#define DATA_SETUP_NS      100  // データ・アドレス設定後のセットアップ待ち (ナノ秒)

// 画面解像度
#define TFT_WIDTH   480
#define TFT_HEIGHT  320

// RGB565 基本カラー定義
#define COLOR_BLACK   0x0000
#define COLOR_BLUE    0x001F
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F
#define COLOR_YELLOW  0xFFE0
#define COLOR_WHITE   0xFFFF

// ------------------------------------------------------------------------------
// バス制御・低レベル関数
// ------------------------------------------------------------------------------

// データバスに8ビット値を出力
inline void setBusData(uint8_t d) {
    digitalWriteFast(TFT_D0, (d >> 0) & 0x01);
    digitalWriteFast(TFT_D1, (d >> 1) & 0x01);
    digitalWriteFast(TFT_D2, (d >> 2) & 0x01);
    digitalWriteFast(TFT_D3, (d >> 3) & 0x01);
    digitalWriteFast(TFT_D4, (d >> 4) & 0x01);
    digitalWriteFast(TFT_D5, (d >> 5) & 0x01);
    digitalWriteFast(TFT_D6, (d >> 6) & 0x01);
    digitalWriteFast(TFT_D7, (d >> 7) & 0x01);
}

// WRストローブを立ち下げ・立ち上げ（確実なラッチ時間を確保）
inline void wrStrobe() {
    delayNanoseconds(DATA_SETUP_NS); // データセットアップ時間 tDST
    digitalWriteFast(TFT_WR, LOW);
    delayNanoseconds(WR_PULSE_DELAY_NS); // tWRL (LOWパルス幅)
    digitalWriteFast(TFT_WR, HIGH);
    delayNanoseconds(WR_PULSE_DELAY_NS); // tWRH (HIGHパルス幅)
}

// コマンド書き込み (DC=LOW)
void writeCommand(uint8_t cmd) {
    digitalWriteFast(TFT_DC, LOW);
    digitalWriteFast(TFT_CS, LOW);
    setBusData(cmd);
    wrStrobe();
    digitalWriteFast(TFT_CS, HIGH);
}

// 8ビットデータ書き込み (DC=HIGH)
void writeData(uint8_t data) {
    digitalWriteFast(TFT_DC, HIGH);
    digitalWriteFast(TFT_CS, LOW);
    setBusData(data);
    wrStrobe();
    digitalWriteFast(TFT_CS, HIGH);
}

// 16ビットデータ書き込み（RGB565カラーデータ用: Highバイト -> Lowバイト）
void writeData16(uint16_t data) {
    digitalWriteFast(TFT_DC, HIGH);
    digitalWriteFast(TFT_CS, LOW);
    
    // High Byte
    setBusData((uint8_t)(data >> 8));
    wrStrobe();
    
    // Low Byte
    setBusData((uint8_t)(data & 0xFF));
    wrStrobe();
    
    digitalWriteFast(TFT_CS, HIGH);
}

// ------------------------------------------------------------------------------
// ハードウェアリセット
// ------------------------------------------------------------------------------
void hardwareReset() {
    Serial.println("[INFO] Hardware Resetting ILI9488...");
    digitalWriteFast(TFT_RST, HIGH);
    delay(10);
    digitalWriteFast(TFT_RST, LOW);
    delay(30);   // リセットパルス幅 (余裕を持って30ms)
    digitalWriteFast(TFT_RST, HIGH);
    delay(120);  // 内部発振器・電源昇圧回路の安定化待ち (120ms)
    Serial.println("[INFO] Reset Completed.");
}

// ------------------------------------------------------------------------------
// ILI9488 8bitパラレル初期化シーケンス
// ------------------------------------------------------------------------------
void initILI9488() {
    Serial.println("[INFO] Sending Init Command Sequence...");

    // 1. ソフトウェアリセット
    writeCommand(0x01);
    delay(150);

    // 2. スリープ解除
    writeCommand(0x11); // Sleep OUT
    delay(150);

    // 3. インターフェース・ピクセルフォーマット (16-bit RGB565)
    writeCommand(0x3A);
    writeData(0x55);    // 16-bit/pixel (2 bytes per pixel)

    // 4. メモリアクセスコントロール (MADCTL: 横向き表示 + BGR)
    writeCommand(0x36);
    writeData(0x28);    // MV=1, BGR=1

    // 5. 表示機能コントロール
    writeCommand(0xB6);
    writeData(0x02);
    writeData(0x02);

    // 6. 電源コントロール
    writeCommand(0xC0); // Power Control 1
    writeData(0x17);
    writeData(0x15);

    writeCommand(0xC1); // Power Control 2
    writeData(0x41);

    writeCommand(0xC5); // VCOM Control
    writeData(0x00);
    writeData(0x12);
    writeData(0x80);

    // 7. スリープ復帰＆表示ON
    writeCommand(0x29); // Display ON
    delay(50);

    Serial.println("[INFO] Init Sequence Completed.");
}

// ------------------------------------------------------------------------------
// 描画ウィンドウ範囲設定
// ------------------------------------------------------------------------------
void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    writeCommand(0x2A); // Column Address Set
    writeData((uint8_t)(x0 >> 8));
    writeData((uint8_t)(x0 & 0xFF));
    writeData((uint8_t)(x1 >> 8));
    writeData((uint8_t)(x1 & 0xFF));

    writeCommand(0x2B); // Page Address Set
    writeData((uint8_t)(y0 >> 8));
    writeData((uint8_t)(y0 & 0xFF));
    writeData((uint8_t)(y1 >> 8));
    writeData((uint8_t)(y1 & 0xFF));

    writeCommand(0x2C); // Memory Write
}

// ------------------------------------------------------------------------------
// 矩形塗りつぶし
// ------------------------------------------------------------------------------
void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if ((x >= TFT_WIDTH) || (y >= TFT_HEIGHT)) return;
    if ((x + w - 1) >= TFT_WIDTH)  w = TFT_WIDTH - x;
    if ((y + h - 1) >= TFT_HEIGHT) h = TFT_HEIGHT - y;

    setAddrWindow(x, y, x + w - 1, y + h - 1);

    digitalWriteFast(TFT_DC, HIGH);
    digitalWriteFast(TFT_CS, LOW);

    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)(color & 0xFF);

    uint32_t totalPixels = (uint32_t)w * h;
    for (uint32_t i = 0; i < totalPixels; i++) {
        setBusData(hi);
        wrStrobe();
        setBusData(lo);
        wrStrobe();
    }

    digitalWriteFast(TFT_CS, HIGH);
}

// 全画面塗りつぶし
void fillScreen(uint16_t color) {
    fillRect(0, 0, TFT_WIDTH, TFT_HEIGHT, color);
}

// ------------------------------------------------------------------------------
// 8色カラーバー描画
// ------------------------------------------------------------------------------
void drawColorBars() {
    Serial.println("[INFO] Drawing 8-Color Bars...");

    const uint16_t colors[8] = {
        COLOR_WHITE,
        COLOR_YELLOW,
        COLOR_CYAN,
        COLOR_GREEN,
        COLOR_MAGENTA,
        COLOR_RED,
        COLOR_BLUE,
        COLOR_BLACK
    };

    uint16_t barWidth = TFT_WIDTH / 8; // 480 / 8 = 60 pixels

    for (int i = 0; i < 8; i++) {
        uint16_t x = i * barWidth;
        uint16_t w = (i == 7) ? (TFT_WIDTH - x) : barWidth;
        fillRect(x, 0, w, TFT_HEIGHT, colors[i]);
    }

    // 外枠・境界（白）
    fillRect(0, 0, TFT_WIDTH, 4, COLOR_WHITE);
    fillRect(0, TFT_HEIGHT - 4, TFT_WIDTH, 4, COLOR_WHITE);

    Serial.println("[INFO] Color Bars Drawn.");
}

// ------------------------------------------------------------------------------
// setup / loop
// ------------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    uint32_t startWait = millis();
    while (!Serial && (millis() - startWait < 3000));

    Serial.println("\n=============================================");
    Serial.println(" Teensy 4.1 - ILI9488 8-bit Parallel Bring-up");
    Serial.printf (" WR Pulse Delay: %d ns\n", WR_PULSE_DELAY_NS);
    Serial.println("=============================================");

    // 制御ピン初期化
    pinMode(TFT_CS, OUTPUT);
    pinMode(TFT_WR, OUTPUT);
    pinMode(TFT_DC, OUTPUT);
    pinMode(TFT_RST, OUTPUT);

    digitalWriteFast(TFT_CS, HIGH);
    digitalWriteFast(TFT_WR, HIGH);
    digitalWriteFast(TFT_DC, HIGH);
    digitalWriteFast(TFT_RST, HIGH);

    // データピン初期化
    pinMode(TFT_D0, OUTPUT);
    pinMode(TFT_D1, OUTPUT);
    pinMode(TFT_D2, OUTPUT);
    pinMode(TFT_D3, OUTPUT);
    pinMode(TFT_D4, OUTPUT);
    pinMode(TFT_D5, OUTPUT);
    pinMode(TFT_D6, OUTPUT);
    pinMode(TFT_D7, OUTPUT);

    setBusData(0x00);

    // ハードウェアリセットと初期化
    hardwareReset();
    initILI9488();

    // 画面クリア（黒）
    Serial.println("[INFO] Clearing Screen to Black...");
    fillScreen(COLOR_BLACK);
    delay(500);

    // カラーバー描画
    drawColorBars();
}

void loop() {
    delay(4000);
    Serial.println("[INFO] Inverting Screen test (0x21 / 0x20)...");
    writeCommand(0x21); // 反転表示 ON
    delay(1000);
    writeCommand(0x20); // 反転表示 OFF
}