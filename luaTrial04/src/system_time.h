#ifndef SYSTEM_TIME_H
#define SYSTEM_TIME_H

#include <Arduino.h>
#include <TimeLib.h>

// NTP同期が完了しているかどうかのフラグ（外部から参照可能にする）
extern bool ntp_synced; 

// 時刻管理システム用API
void init_system_time();
time_t getTeensy3Time();
bool syncWithNTP();

#endif