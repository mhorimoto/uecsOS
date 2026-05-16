#ifndef LIBUECS_H
#define LIBUECS_H
#define LIBUECS_H_VERSION "0.0.5"
struct CCMData {
    char type;         // Receive,Send,Request,Response etc.
    char ccmtype[21];      // InAirTemp,InAirHumid,WAirTemp,InAirCO2, etc.
    uint8_t room;            // 部屋番号 0は全ての部屋-127まで
    uint8_t region;          // 系統番号 0は全ての系統-127まで
    uint16_t order;           // 通し番号 0は全ての装置向け-30000まで
    uint8_t priority;        // 優先度 0-30 (数値が小さいほど優先度が高い)
    float value;         // 物理量
    uint8_t num_digit;    // 桁数
    uint8_t decimal_places; // 小数点以下の桁数
    uint16_t interval_sec;  // 送信周期、または期待される受信周期（秒）
    uint32_t last_update_ms;// 最後に送信・受信した時刻(millis)
    bool valid = false;     // 【R専用】データが寿命内（有効）かどうか
    bool active = false;    // スロット自体が使用中かどうか
    // --- cnd スロット(0番)専用のデータ保持 ---
    uint32_t cnd_value = 0; // 欠落のない32bit整数
    bool cnd_is_hex = false; // HEX表記("0x...")で出力するかのフラグ
};

#define MAX_UECS_SLOTS 10
extern CCMData uecs_slots[MAX_UECS_SLOTS];

// APIプロトタイプ (interval_secを追加)
bool set_uecs_slot_internal(char p_type, const char* p_ccmtype, uint8_t room, uint8_t region, uint16_t order, uint8_t priority, float value, uint8_t num_digit, uint8_t decimal_places, uint16_t interval_sec);

// --- UECSネットワーク管理用 ---
void init_uecs_network();           // UDP 16520ポートのバインド
void execute_uecs_transmission();   // 送信(S)スロットの処理
void process_incoming_uecs();       // 受信(R)スロットの処理
void check_uecs_lifespan();         // 寿命監視用関数

#endif