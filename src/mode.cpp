#include "mode.h"
#include "led.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>

constexpr TickType_t PERIOD = pdMS_TO_TICKS(100);
volatile BootMode gBootMode;

void readMode() {
    pinMode(MODE1, INPUT_PULLDOWN);
    pinMode(MODE2, INPUT_PULLDOWN);
    pinMode(MODE3, INPUT_PULLDOWN);
    uint8_t m1 = digitalRead(MODE1);
    uint8_t m2 = digitalRead(MODE2);
    uint8_t m3 = digitalRead(MODE3);
    if (m1) {
        gBootMode = BootMode::KEYBOARD;
    } else if (m2) {
        gBootMode = BootMode::MACRO;
    } else if (m3) {
        gBootMode = BootMode::METRONOME;
    }
}

void ModeTask(void*) {
    for (;;) {
        auto last = gBootMode;
        readMode();
        if (last != gBootMode) {
            fillDark();
            esp_restart();
        }
        vTaskDelay(PERIOD);
    }
}

void setupMode() {
    readMode();
    xTaskCreatePinnedToCore(
        ModeTask,
        "ModeTask",
        2048,
        nullptr,
        1,
        nullptr,
        APP_CPU_NUM
    );
}
