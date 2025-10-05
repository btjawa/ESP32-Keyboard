#include "mode.h"
#include "ota.h"
#include "battery.h"

#include "keyboard.h"
#include "metronome.h"
#include "led.h"

void setup(void) {
    Serial.begin(115200);

    /* MODE */
    setupMode();

    /* OTA */
    setupOTA();

    /* BATTERY */
    setupBattery();

    /* KEYBOARD */
    setupKeyboard();

    /* METRONOME */
    setupMetronome();

    /* LED */
    setupLED();
}

void loop() {
    


}
