#include "metronome.hpp"

extern "C" {
    #include <driver/ledc.h>
    #include <driver/gpio.h>
}

#define BUZZER_TIMER LEDC_TIMER_0
#define BUZZER_MODE LEDC_LOW_SPEED_MODE
#define BUZZER_HZ 2000
#define BUZZER_RES LEDC_TIMER_10_BIT
#define BUZZUER_CH LEDC_CHANNEL_0
#define BUZZER_PIN 8

void setupMetronome() {
    ledc_timer_config_t tcfg = {
        .speed_mode = BUZZER_MODE,
        .duty_resolution = BUZZER_RES,
        .timer_num = BUZZER_TIMER,
        .freq_hz = BUZZER_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&tcfg));
    ledc_channel_config_t ccfg = {
        .gpio_num = BUZZER_PIN,
        .speed_mode = BUZZER_MODE,
        .channel = BUZZUER_CH,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = BUZZER_TIMER,
        .duty = 0,
        .hpoint = 0,
        .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
        .flags = {
            .output_invert = 0
        },
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ccfg));
}
