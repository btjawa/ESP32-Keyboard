#pragma once
#include <Arduino.h>

#include <BleKeyboard.h>

namespace ble_hid {
    void setup();
    void press(const uint8_t key);
    void write(const MediaKeyReport& key);
    void release(const uint8_t key);
}
