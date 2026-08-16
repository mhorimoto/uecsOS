#ifndef LIBUECS_H
#define LIBUECS_H
#define LIBUECS_H_VERSION "0.0.1"
struct CCMData {
    char type;         // Receive,Send,Request,Response etc.
    char ccmtype[21];      // InAirTemp,InAirHumid,WAirTemp,InAirCO2, etc.
    uint8_t room;            // 部屋番号 0は全ての部屋-127まで
    uint8_t region;          // 系統番号 0は全ての系統-127まで
    uint16_t order;           // 通し番号 0は全ての装置向け-30000まで
    uint8_t priority;        // 優先度 0-30 (数値が小さいほど優先度が高い)
    float value;         // 物理量
    uint8_t lifespan;     // 有効寿命 (最大180秒) : 1バイトを確保
    uint8_t num_digit;    // 桁数
    uint8_t decimal_places; // 小数点以下の桁数
    bool active = false;  // データの有効/無効フラグ（trueなら有効、falseなら無効）
};

#define MAX_UECS_SLOTS 10

#endif