#pragma once
#include <Arduino.h>

namespace usb_hid {
    void setup();
    void press(const uint8_t key);
    void write(uint16_t usage);
    void release(const uint8_t key);
}
