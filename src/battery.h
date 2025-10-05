#pragma once

#include <Arduino.h>

struct BatStatus {
    uint16_t last_mV = 0;
    uint16_t ema_q8 = 0;
    uint8_t avgPct = 0;
    uint8_t inited = 0;
};

extern volatile BatStatus gBat;

void setupBattery();
