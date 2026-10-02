#ifndef ILI9488_LCD_H
#define ILI9488_LCD_H
#include <Arduino_GFX_Library.h>
// --------------------------------------------------
// ILI9488 8ビットパラレル ピン定義
// --------------------------------------------------
#define TFT_RST  GFX_NOT_DEFINED
#define TFT_CS   10
#define TFT_WR   14
#define TFT_DC   15

#define TFT_D0   28
#define TFT_D1   29
#define TFT_D2   30
#define TFT_D3   31
#define TFT_D4   35
#define TFT_D5   34
#define TFT_D6   40
#define TFT_D7   41

// Adafruit_GFX互換カラー定数
#ifndef BLACK
#define BLACK   RGB565_BLACK
#define WHITE   RGB565_WHITE
#define RED     RGB565_RED
#define GREEN   RGB565_GREEN
#define BLUE    RGB565_BLUE
#define YELLOW  RGB565_YELLOW
#define CYAN    RGB565_CYAN
#define MAGENTA RGB565_MAGENTA
#endif

#endif
