#pragma once

#include <Arduino.h>

#define MODE1 40
#define MODE2 41
#define MODE3 42
enum class BootMode: uint8_t {
    KEYBOARD,
    MACRO,
    METRONOME,
};

extern volatile BootMode gBootMode;

void setupMode();
