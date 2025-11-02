#pragma once
#include <cstdint>

enum class BootMode : uint8_t {
    KEYBOARD,
    MACRO,
    METRONOME,
};

extern volatile BootMode gBootMode;

void setupMode();
