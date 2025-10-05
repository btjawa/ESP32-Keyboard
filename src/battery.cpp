#include "battery.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define BATTERY_ADC 6
#define BAT_DIV_NUM 2
#define BAT_DIV_DEN 1

constexpr TickType_t PERIOD = pdMS_TO_TICKS(100);

struct Knot { uint16_t mv; float pct; };
static const Knot kSoc[22] {
    {4190,100},
    {4080,93},
    {4020,88},
    {3975,84},
    {3935,79},
    {3900,74},
    {3865,69},
    {3845,64},
    {3810,58},
    {3790,54},
    {3765,49},
    {3745,45},
    {3725,41},
    {3705,36},
    {3685,31},
    {3665,27},
    {3640,21},
    {3570,13},
    {3490,8},
    {3410,5},
    {3050,1},
    {3000,0},
};
static const uint8_t kSocLen = sizeof(kSoc) / sizeof(kSoc[0]);

volatile BatStatus gBat; 

static uint16_t readVolts() {
    const uint8_t N = 8;
    uint32_t sum = 0;
    uint32_t vmin = UINT32_MAX;
    uint32_t vmax = 0;

    for (uint8_t i = 0; i < N; ++i) {
        uint32_t mV = analogReadMilliVolts(BATTERY_ADC);
        sum += mV;
        if (mV < vmin) vmin = mV;
        if (mV > vmin) vmax = mV;
    }

    sum -= vmin;
    sum -= vmax;
    uint32_t avg = sum / (N - 2);
    uint32_t vbat = (avg * BAT_DIV_NUM) / BAT_DIV_DEN;
    if (vbat > 65535) vbat = 65535;
    return (uint16_t)vbat;
}

static uint8_t readPct(uint16_t mV) {
    if (mV >= kSoc[0].mv) {
        return kSoc[0].mv;
    }
    if (mV <= kSoc[kSocLen - 1].mv) {
        return kSoc[kSocLen - 1].mv;
    }

    uint16_t lo = 0, hi = kSocLen - 1;
    while (hi - lo > 1) {
        auto mid = (lo + hi) >> 1;
        if (mV > kSoc[mid].mv) {
            hi = mid;
        } else {
            lo = mid;
        }
    }
    const auto &H = kSoc[lo];
    const auto &L = kSoc[hi];

    uint32_t span_mv = H.mv - L.mv;
    uint32_t off_mv = mV - L.mv;
    uint32_t span_pct_q8 = (uint32_t)(H.pct - L.pct) * 256;
    uint32_t interp_q8 = span_pct_q8 * off_mv / span_mv;
    uint32_t pct_q8 = (uint32_t)L.pct * 256 + interp_q8;

    return (uint8_t)((pct_q8 + 128u) >> 8);
}

static void BatteryTask(void*) {
    TickType_t lastWake = xTaskGetTickCount();
    for (;;) {
        auto mV = readVolts();
        auto pct = readPct(mV);
        if (!gBat.inited) {
            gBat.inited = 1;
            gBat.ema_q8 = pct * 256;
            gBat.avgPct = pct;
        } else {
            int32_t delta = (int32_t)(pct * 256) - (int32_t)gBat.ema_q8;
            gBat.ema_q8 = (uint16_t)((int32_t)gBat.ema_q8 + (delta >> 4)); // EMA_SHIFT=4
            gBat.avgPct = (uint8_t)((gBat.ema_q8 + 128) >> 8);
        }
        gBat.last_mV = mV;
        vTaskDelayUntil(&lastWake, PERIOD);
    }
}

void setupBattery() {
    adcAttachPin(BATTERY_ADC);
    analogReadResolution(12);
    analogSetPinAttenuation(BATTERY_ADC, ADC_11db);
    xTaskCreatePinnedToCore(
        BatteryTask,
        "BatteryTask",
        2048,
        nullptr,
        1,
        nullptr,
        tskNO_AFFINITY
    );
}
