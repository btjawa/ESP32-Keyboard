#include "mode.h"

volatile BootMode gBootMode;

void setupMode() {
    pinMode(MODE1, INPUT_PULLDOWN);
    pinMode(MODE2, INPUT_PULLDOWN);
    pinMode(MODE3, INPUT_PULLDOWN);
    uint8_t m1 = digitalRead(MODE1);
    uint8_t m2 = digitalRead(MODE2);
    uint8_t m3 = digitalRead(MODE3);
    if (m1) {
        gBootMode = BootMode::USB_KEYBOARD;
    } else if (m2) {
        gBootMode = BootMode::BLE_KEYBOARD;
    } else if (m3) {
        gBootMode = BootMode::METRONOME;
    }
}
