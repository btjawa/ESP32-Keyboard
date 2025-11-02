#pragma once

#include <cstdint>

namespace ble_hid {
    void setup(char serial_str[17]);
    void end();
    void press(const uint8_t key);
    void release(const uint8_t key);
}
