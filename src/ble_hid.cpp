#include "ble_hid.h"
#include "battery.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

constexpr TickType_t PERIOD = pdMS_TO_TICKS(5000);

BleKeyboard bleKeyboard;

static void updateBattery(void*) {
    for (;;) {
        bleKeyboard.setBatteryLevel(gBat.avgPct);
        vTaskDelay(PERIOD);
    }
}

namespace ble_hid {
    void setup() {
        bleKeyboard.begin();
        xTaskCreatePinnedToCore(
            updateBattery,
            "ble_hid::updateBattery",
            6144,
            nullptr,
            1,
            nullptr,
            APP_CPU_NUM
        );
    };
    void press(const uint8_t key) {
        bleKeyboard.press(key);
    }
    void write(const MediaKeyReport& key) {
        bleKeyboard.press(key);
        bleKeyboard.release(key);
    }
    void release(const uint8_t key) {
        bleKeyboard.release(key);
    }
}
