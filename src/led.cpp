#include "led.h"
#include "keyboard.h"

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

#define LED_BRT 60
#define LED_BPM 30
#define LED_PHASE 32
#define LED_MIN 32
#define LED_MAX 128
#define LED_PLD_MIN 16
#define LED_PLD_MAX 32

constexpr TickType_t PERIOD = pdMS_TO_TICKS(16);

static void LEDTask(void*) {
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        /* WAVING LIGHTS */
        for (uint8_t col = 0; col < LED_COLS; ++col) {
            const uint8_t phase = col * LED_PHASE;
            uint8_t level = beatsin8(LED_BPM, LED_MIN, LED_MAX, 0, phase);
            for (uint8_t row = 0; row < LED_ROWS; ++row) {
                CRGB c = CRGB(0x00, 0xBC, 0xD4);
                c.nscale8_video(level);
                led_rows[row][col] = c;
            }
        }

        /* KEYS HIGHLIGHTING LIGHTS */
        for (uint8_t row = 0; row < LED_ROWS; ++row) {
            for (uint8_t col = 0; col < LED_COLS; ++col) {
                if (gKeyPressed[row][col]) {
                    CRGB c = CRGB(0x00, 0xBC, 0xD4);
                    led_rows[row][col] = c;
                }
            }
        }
        
        /* PLD LIGHTS */
        for (uint8_t i = 1; i <= 3; ++i) {
            const uint8_t phase = i * LED_PHASE;
            uint8_t level = beatsin8(LED_BPM, LED_PLD_MIN, LED_PLD_MAX, 0, phase);
            CRGB c = CRGB(0x00, 0xBC, 0xD4);
            c.nscale8_video(level);
            led_pld[i] = c;
        }

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
    FastLED.setCorrection(TypicalLEDStrip);
    FastLED.setMaxRefreshRate(120, true);
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
