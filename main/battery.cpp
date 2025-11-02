#include "battery.hpp"

extern "C" {
    #include <freertos/FreeRTOS.h>
    #include <freertos/task.h>
    #include <esp_system.h>
    #include <esp_adc/adc_oneshot.h>
    #include <esp_adc/adc_cali.h>
    #include <esp_adc/adc_cali_scheme.h>
    #include <driver/gpio.h>
}

// GPIO2
#define ADC_CHANNEL ADC_CHANNEL_1
#define ADC_ATTEN ADC_ATTEN_DB_12
#define ADC_BITWIDTH ADC_BITWIDTH_12
#define BAT_DIV_NUM 2
#define BAT_DIV_DEN 1

#define CHRG_PIN GPIO_NUM_38

struct CurveNode {
    uint16_t mV;
    float pct;
};

static constexpr CurveNode Chrg[13] {
    {3000, 0},  {3200, 5},  {3350,10}, {3450,15}, {3550,20},
    {3630,30},  {3700,40},  {3770,50}, {3820,60}, {3870,70},
    {3920,80},  {4050,90},  {4200,100}
};

static constexpr CurveNode DisChrg[13] {
    {3000, 0},  {3230, 5},  {3380,10}, {3480,15}, {3600,20},
    {3680,30},  {3750,40},  {3820,50}, {3860,60}, {3910,70},
    {3960,80},  {4080,90},  {4200,100}
};

volatile BatStatus gBat;

static adc_oneshot_unit_handle_t s_adc = nullptr;
static adc_cali_handle_t s_cali = nullptr;

bool inline isChrging() {
    return !gpio_get_level(CHRG_PIN);
}

static uint16_t readVolts() {
    const uint8_t N = 8;
    uint32_t sum = 0;
    uint32_t vmin = UINT32_MAX;
    uint32_t vmax = 0;

    for (uint8_t i = 0; i < N; ++i) {
        auto raw = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(s_adc, ADC_CHANNEL, &raw));
        auto mv = 0;
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(s_cali, raw, &mv));
        uint32_t mV = (uint16_t)mv;
        sum += mV;
        if (mV < vmax) vmin = mV;
        if (mV > vmin) vmax = mV;
    }

    sum -= vmin;
    sum -= vmax;
    uint32_t avg = sum / (N - 2);
    uint32_t vbat = (avg * BAT_DIV_NUM) / BAT_DIV_DEN;
    if (vbat > 65535) vbat = 65535;
    return (uint16_t)vbat;
}

static uint8_t readPct(uint16_t mV, bool isChrging) {
    auto curve = isChrging ? Chrg : DisChrg;
    auto len = 13;
    if (mV <= curve[0].mV) {
        return curve[0].pct;
    }
    if (mV >= curve[len - 1].mV) {
        return curve[len - 1].pct;
    }

    uint16_t lo = 0, hi = len - 1;
    while (hi - lo > 1) {
        auto mid = (lo + hi) >> 1;
        if (mV < curve[mid].mV) {
            hi = mid;
        } else {
            lo = mid;
        }
    }
    const auto &H = curve[lo];
    const auto &L = curve[hi];

    uint32_t span_mv = H.mV - L.mV;
    uint32_t off_mv = mV - L.mV;
    uint32_t span_pct_q8 = (uint32_t)(H.pct - L.pct) * 256;
    uint32_t interp_q8 = span_pct_q8 * off_mv / span_mv;
    uint32_t pct_q8 = (uint32_t)L.pct * 256 + interp_q8;

    return (uint8_t)((pct_q8 + 128u) >> 8);
}

static void BatteryTask(void*) {
    const TickType_t interval = pdMS_TO_TICKS(100);
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        auto chrging = isChrging();
        gBat.isChrging = chrging;

        auto mV = readVolts();
        auto pct = readPct(mV, chrging);
        if (!gBat.inited) {
            gBat.inited = true;
            gBat.ema_q8 = pct * 256;
            gBat.avgPct = pct;
        } else {
            int32_t delta = (int32_t)(pct * 256) - (int32_t)gBat.ema_q8;
            gBat.ema_q8 = (uint16_t)((int32_t)gBat.ema_q8 + (delta >> 4)); // EMA_SHIFT=4
            gBat.avgPct = (uint8_t)((gBat.ema_q8 + 128) >> 8);
        }
        gBat.last_mV = mV;
        vTaskDelayUntil(&last, interval);
    }
}

void setupBattery()  {
    gpio_reset_pin(CHRG_PIN);
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << CHRG_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);

    adc_oneshot_unit_init_cfg_t unit_cfg = {};
    unit_cfg.unit_id = ADC_UNIT_1;
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_adc));

    adc_oneshot_chan_cfg_t chan_cfg = {};
    chan_cfg.atten = ADC_ATTEN;
    chan_cfg.bitwidth = ADC_BITWIDTH;
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc, ADC_CHANNEL, &chan_cfg));

    adc_cali_curve_fitting_config_t cali_cfg = {};
    cali_cfg.unit_id = ADC_UNIT_1;
    cali_cfg.atten = ADC_ATTEN;
    cali_cfg.bitwidth = ADC_BITWIDTH;
    adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali);

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
