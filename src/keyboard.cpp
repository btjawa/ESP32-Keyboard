#include "keyboard.h"
#include "usb_hid.h"
#include "ble_hid.h"
#include "mode.h"

#include "ota.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <ESP32Encoder.h>

const TickType_t PERIOD = pdMS_TO_TICKS(5);

const uint8_t KeyMap[K_ROWS_LEN][K_COLS_LEN] = {
    { 0xE7, 0xE8, 0xE9, 0xB1, },
    { 0xE4, 0xE5, 0xE6, 0xDF, },
    { 0xE1, 0xE2, 0xE3, 0xDE, },
    { 0xEA, 0xDD, 0xDC, 0xE0, },
};

volatile bool gKeyPressed[K_ROWS_LEN][K_COLS_LEN] = {false};

int64_t lastCount = 0;
int8_t COUNTS_PER_DETENT = 2;

#define ENC_A 39
#define ENC_B 38
ESP32Encoder encoder;

void resetKeys() {
    for (uint8_t i = 0; i < K_ROWS_LEN; i++) pinMode(K_ROWS[i], INPUT);
    for (uint8_t i = 0; i < K_COLS_LEN; i++) pinMode(K_COLS[i], INPUT_PULLDOWN);
}

void press(const uint8_t key) {
    if (gBootMode == BootMode::USB_KEYBOARD)
        usb_hid::press(key);
    else if (gBootMode == BootMode::BLE_KEYBOARD)
        ble_hid::press(key);
}

void release(const uint8_t key) {
    if (gBootMode == BootMode::USB_KEYBOARD)
        usb_hid::release(key);
    else if (gBootMode == BootMode::BLE_KEYBOARD)
        ble_hid::release(key);
}

void incVol() {
    if (gBootMode == BootMode::USB_KEYBOARD)
        usb_hid::write(0x00E9);
    else if (gBootMode == BootMode::BLE_KEYBOARD)
        ble_hid::write(KEY_MEDIA_VOLUME_UP);
}

void decVol() {
    if (gBootMode == BootMode::USB_KEYBOARD)
        usb_hid::write(0x00EA);
    else if (gBootMode == BootMode::BLE_KEYBOARD)
        ble_hid::write(KEY_MEDIA_VOLUME_DOWN);
}

void KeysTask(void*) {
    TickType_t lastWake = xTaskGetTickCount();
    for (;;) {
        for (uint8_t i = 0; i < K_ROWS_LEN; i++) {
            pinMode(K_ROWS[i], OUTPUT);
            digitalWrite(K_ROWS[i], HIGH);
            delayMicroseconds(10);
            for (uint8_t j = 0; j < K_COLS_LEN; j++) {
                bool pressed = (digitalRead(K_COLS[j]) == HIGH);
                bool wasPressed = gKeyPressed[i][j];
                uint8_t key = KeyMap[i][j];
                if (pressed && !wasPressed) {
                    press(key);
                } else if (!pressed && wasPressed) {
                    release(key);
                    if (i == 0 && j == 3) checkOTA();
                }
                gKeyPressed[i][j] = pressed;
            }
            pinMode(K_ROWS[i], INPUT);
        }
        int64_t c = encoder.getCount();
        int64_t steps = (c - lastCount) / COUNTS_PER_DETENT;

        if (steps != 0) {
            lastCount += (int64_t)steps * COUNTS_PER_DETENT;
            auto n = (steps > 0) ? steps : -steps;
            for (int i = 0; i < n; ++i) {
                (steps > 0) ? incVol() : decVol();
                vTaskDelay(pdMS_TO_TICKS(5));
            }
        }
        resetKeys();
        vTaskDelayUntil(&lastWake, PERIOD);
    }
}

void setupKeyboard() {
    if (gBootMode == BootMode::USB_KEYBOARD) {
        usb_hid::setup();
    } else if (gBootMode == BootMode::BLE_KEYBOARD) {
        ble_hid::setup();
    }
    pinMode(ENC_A, INPUT_PULLUP);
    pinMode(ENC_B, INPUT_PULLUP);
    ESP32Encoder::useInternalWeakPullResistors = puType::up;
    encoder.attachFullQuad(ENC_A, ENC_B);
    encoder.clearCount();
    resetKeys();
    xTaskCreatePinnedToCore(
        KeysTask,
        "KeysTask",
        4096,
        nullptr,
        2,
        nullptr,
        APP_CPU_NUM
    );
}
