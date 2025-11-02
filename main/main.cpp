#include "mode.hpp"
#include "ota.hpp"
#include "battery.hpp"
#include "keyboard.hpp"
#include "metronome.hpp"
#include "led.hpp"

extern "C" void app_main(void) {
    setupKeyboard();
    setupMode();
    setupOTA();
    setupBattery();
    setupMetronome();
    setupLED();
}
