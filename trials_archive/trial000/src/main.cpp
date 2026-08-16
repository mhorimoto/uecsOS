#include <Arduino.h>   // Serial, NULL のために必要
#include <stdarg.h>    // va_list, va_start, va_end のために必要
#include <stdio.h>     // vsnprintf のために必要
#include "my_basic.h"

// 標準出力（PRINT文）の転送先
void _print_handler(struct mb_interpreter_t* s, const char* fmt, ...) {
  char buf[128];
  va_list arg;
  va_start(arg, fmt);
  vsnprintf(buf, sizeof(buf), fmt, arg);
  va_end(arg);
  Serial.print(buf);
}

struct mb_interpreter_t* bas = NULL;

void setup() {
  Serial.begin(115200);
  while (!Serial); // シリアル準備待ち

  mb_init();
  mb_open(&bas);
  
  // PRINT文の出力先をシリアルに設定
  mb_set_printer(bas, _print_handler);

  // テストプログラムの実行
  const char* code = "print \"MY-BASIC on Teensy 4.1 starting...\"\n"
                     "for i = 1 to 5\n"
                     "  print \"Count: \", i\n"
                     "next i\n";
  
  mb_load_string(bas, code, true);
  mb_run(bas, true);
}

void loop() {
  // ここにコマンド入力処理などを実装
}