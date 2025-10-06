#include "led.h"
#include "keyboard.h"
#include "mode.h"

#include <FastLED.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define LED_PLD 3
#define LED_PLDS 7
CRGB led_pld[7];

#define LED_ROW1 18
#define LED_ROW2 17
#define LED_ROW3 16
#define LED_ROW4 15
#define LED_ROWS 4
#define LED_COLS 4
CRGB led_rows[LED_ROWS][LED_COLS];

#define LED_BRT 30
#define LED_BPM 45

constexpr uint16_t LED_PHASE = 32 * 256;
constexpr uint16_t LED_MIN = 63 * 256;
constexpr uint16_t LED_MAX = 255 * 256;
constexpr uint8_t LED_PLD_MIN = 31;
constexpr uint8_t LED_PLD_MAX = 63;

constexpr TickType_t PERIOD = pdMS_TO_TICKS(4);

static void LEDTask(void*) {
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        /* WAVING LIGHTS */
        for (auto col = 0; col < LED_COLS; ++col) {
            uint16_t phase = col * LED_PHASE;
            uint16_t level = beatsin16(LED_BPM, LED_MIN, LED_MAX, 0, phase);
            for (auto row = 0; row < LED_ROWS; ++row) {
                CRGB c = CRGB(0x00, 0xBC, 0xD4);
                c.nscale8_video(level >> 8);
                led_rows[row][col] = c;
            }
        }

        /* KEYS HIGHLIGHTING LIGHTS */
        for (auto row = 0; row < LED_ROWS; ++row) {
            for (auto col = 0; col < LED_COLS; ++col) {
                if (gKeyPressed[row][col]) {
                    CRGB c = CRGB(0x00, 0xBC, 0xD4);
                    led_rows[row][col] = c;
                }
            }
        }
        
        /* PLD LIGHTS */
        uint8_t m = (uint8_t)(gBootMode) + 1;
        uint8_t t = (uint8_t)((beat16(LED_BPM) + 16384 + 0x80) >> 8);
        uint8_t level = scale8_video(LED_PLD_MAX, 255 - t);
        CRGB c = CRGB(0x00, 0xBC, 0xD4);
        c.nscale8_video(level);
        led_pld[m] = c;

        FastLED.show();
        vTaskDelayUntil(&last, PERIOD);
    }
}

void setupLED() {
    pinMode(45, OUTPUT);
    digitalWrite(45, HIGH);
    FastLED.addLeds<WS2812B, LED_PLD, GRB>(led_pld, LED_PLDS);
    FastLED.addLeds<WS2812B, LED_ROW1, GRB>(led_rows[0], LED_COLS);
    FastLED.addLeds<WS2812B, LED_ROW2, GRB>(led_rows[1], LED_COLS);
    FastLED.addLeds<WS2812B, LED_ROW3, GRB>(led_rows[2], LED_COLS);
    FastLED.addLeds<WS2812B, LED_ROW4, GRB>(led_rows[3], LED_COLS);
    FastLED.setBrightness(LED_BRT);
    FastLED.setDither(true);
    FastLED.setCorrection(TypicalLEDStrip);
    FastLED.clear(true);
    xTaskCreatePinnedToCore(
        LEDTask,
        "LEDTask",
        4096,
        nullptr,
        2,
        nullptr,
        APP_CPU_NUM
    );
}
