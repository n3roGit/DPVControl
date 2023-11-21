#line 1 "C:\\Users\\christoph.bubeck\\Documents\\GitHub\\DPVControl\\DPVController\\Blinker.cpp"
#include "Blinker.h"
#include "Arduino.h"
#include "constants.h"

Blinker::Blinker(void (*onFunction)(), void (*offFunction)()) {
    turnOnFunction = onFunction;
    turnOffFunction = offFunction;
    stopAt = NEVER;
}

void Blinker::blink(long ms) {
    turnOnFunction();
    stopAt = millis() + ms;
}

void Blinker::loop() {
    if (stopAt != NEVER && millis() >= stopAt) {
        turnOffFunction();
        stopAt = NEVER;
    }
}
