#include "Blinker.h"
#include "Arduino.h"

const int NEVER = -1;

Blinker::Blinker(void (*onFunction)(), void (*offFunction)()) {
    turnOnFunction = onFunction;
    turnOffFunction = offFunction;
    stopAt = NEVER;
}

void Blinker::blink(long ms) {
    turnOnFunction();
    stopAt = millis()+ms;
}

void Blinker::loop() {
    if (stopAt != NEVER && millis() >= stopAt) {
        turnOffFunction();
        stopAt = NEVER;
    }
}
