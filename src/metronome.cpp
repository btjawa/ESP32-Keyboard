#include "metronome.h"

void setupMetronome() {
    ledcSetup(BUZZER_CH, 2000, BUZZER_RES);
    ledcAttachPin(BUZZER, BUZZER_CH);
}
