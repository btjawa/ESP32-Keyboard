#include "led.hpp"
#include "mode.hpp"

extern "C" {
    #include <freertos/FreeRTOS.h>
    #include <freertos/task.h>
    #include <led_strip.h>
    #include <driver/gpio.h>
    #include <esp_timer.h>
    #include <esp_log.h>
}

#include <cmath>

#define LED_PWR_EN_PIN GPIO_NUM_45

constexpr uint8_t LED_PLDS_LEN = 6;
constexpr uint8_t LED_PLDS_PIN = 18;

static led_strip_handle_t led_plds = nullptr;

constexpr uint8_t LED_MAIN_COLS = 4;
constexpr uint8_t LED_MAIN_ROWS = 4;
constexpr uint8_t LED_MAIN_LEN = LED_MAIN_ROWS * LED_MAIN_COLS;
constexpr uint8_t LED_MAIN_PIN = 46;

static led_strip_handle_t led_main = nullptr;

constexpr uint8_t LED_BRT = 30;
constexpr uint8_t LED_BPM = 45;
constexpr uint8_t LED_PHASE = 32;
constexpr uint8_t LED_MIN = 63;
constexpr uint8_t LED_MAX = 255;

bool gFillDark = false;

static void new_led(uint8_t gpio, uint16_t len, led_strip_handle_t* out, bool dma = false) {
    led_strip_config_t strip_config = {
        .strip_gpio_num = gpio,
        .max_leds = len,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        }
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 48,
        .flags = {
            .with_dma = dma,
        }
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, out));
}

static void set_pixel(led_strip_handle_t strip, uint16_t idx, uint8_t rgb[3], uint8_t scale) {
    uint8_t rgb_t[3];
    for (uint8_t i = 0; i < 3; i++) {
        uint16_t res = (uint16_t)rgb[i] * (uint16_t)scale;
        res >>= 8;
        if (rgb[i] && scale) res += 1;
        if (res > 255) res = 255;
        uint32_t scaled = (uint32_t)res * LED_BRT / 255;
        rgb_t[i] = (uint8_t)(scaled);
    }
    led_strip_set_pixel(strip, idx, rgb_t[0], rgb_t[1], rgb_t[2]);
}

static void clear_led() {
    led_strip_clear(led_main);
    led_strip_clear(led_plds);
}

void fillDark() {
    gFillDark = true;
    clear_led();
    gpio_set_level(LED_PWR_EN_PIN, 0);
}

void restoreLED() {
    gpio_set_level(LED_PWR_EN_PIN, 1);
    clear_led();
    gFillDark = false;
}

static uint8_t beat8(uint16_t bpm) {
    uint32_t ms = (uint32_t)(esp_timer_get_time() / 1000);
    uint32_t v = (uint32_t)((uint64_t)ms * bpm * 256ull / 60000ull);
    return (uint8_t)v;
}

static uint8_t beatsin8(uint16_t bpm, uint8_t lo, uint8_t hi, uint8_t phase = 0) {
    uint8_t b = beat8(bpm) + phase;
    float theta = (float)b * (2.0f * (float)M_PI / 256.0f);
    uint8_t s = (uint8_t)lroundf((0.5f + 0.5f * sinf(theta)) * 255.0f);
    uint16_t span = (uint16_t)(hi - lo);
    return (uint8_t)(lo + (uint16_t)s * span / 255);
}

static void LEDTask(void*) {
    constexpr TickType_t period = pdMS_TO_TICKS(4);
    TickType_t last = xTaskGetTickCount();
    uint8_t rgb[3] = {0x00, 0xBC, 0xD4};
    for (;;) {
        if (gFillDark) {
            vTaskDelayUntil(&last, period);
            continue;
        }
        /* WAVING LIGHTS */
        for (uint8_t col = 0; col < LED_MAIN_COLS; ++col) {
            uint16_t phase = col * LED_PHASE;
            uint8_t level = beatsin8(LED_BPM, LED_MIN, LED_MAX, phase);
            for (uint8_t row = 0; row < LED_MAIN_ROWS; ++row) {
                set_pixel(led_main, (col * LED_MAIN_COLS) + row, rgb, level);
            }
        }
        /* KEYS HIGHLIGHTING LIGHTS */
        /* TODO */

        /* PLD LIGHTS */
        uint8_t m = (uint8_t)(gBootMode);
        set_pixel(led_plds, m, rgb, ((255 - (uint8_t)beat8(LED_BPM)) * 0.5f));

        led_strip_refresh(led_main);
        led_strip_refresh(led_plds);
        vTaskDelayUntil(&last, period);
    }
}

void setupLED() {
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << LED_PWR_EN_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);

    new_led(LED_MAIN_PIN, LED_MAIN_LEN, &led_main);
    new_led(LED_PLDS_PIN, LED_PLDS_LEN, &led_plds, true);
    
    restoreLED();
    xTaskCreatePinnedToCore(
        LEDTask,
        "LEDTask",
        8192,
        nullptr,
        2,
        nullptr,
        APP_CPU_NUM
    );
}
