#ifndef SHELL_SYSTEM_H
#define SHELL_SYSTEM_H

#include <Arduino.h>

// シリアルからの入力を監視・処理する従来の関数
void process_serial_shell();

// 1行のコマンドを解析し、指定された出力ストリーム(SerialやUDP)に結果を返す統合関数
void execute_shell_command(String line, Print& out);

#endif