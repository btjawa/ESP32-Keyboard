#include "mode.hpp"
#include "led.hpp"

extern "C" {
    #include <freertos/FreeRTOS.h>
    #include <freertos/task.h>
    #include <esp_system.h>
    #include <driver/gpio.h>
}

#define MODE1 GPIO_NUM_42
#define MODE2 GPIO_NUM_41
#define MODE3 GPIO_NUM_40

volatile BootMode gBootMode;

void readMode() {
    int m1 = gpio_get_level(MODE1);
    int m2 = gpio_get_level(MODE2);
    int m3 = gpio_get_level(MODE3);
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
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void setupMode() {
    gpio_reset_pin(MODE1);
    gpio_reset_pin(MODE2);
    gpio_reset_pin(MODE3);
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << MODE1) | (1ULL << MODE2) | (1ULL << MODE3),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);

    readMode();
    xTaskCreatePinnedToCore(
        ModeTask,
        "ModeTask",
        4096,
        nullptr,
        1,
        nullptr,
        APP_CPU_NUM
    );
}
