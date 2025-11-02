#pragma once
#include <cstdint>

struct BatStatus {
    uint16_t last_mV = 0;
    uint16_t ema_q8 = 0;
    uint8_t avgPct = 0;
    bool isChrging = false;
    bool inited = false;
};

extern volatile BatStatus gBat;

void setupBattery();
