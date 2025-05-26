#include "BlinkSequence.h"

BlinkSequence::BlinkSequence(
  Blinker &blinker, 
  long (*durationFunction)(char),
  long pauseDuration) : blinker(blinker) {
    this->durationFunction = durationFunction;
    this->pauseDuration = pauseDuration;
    sequence = "";
    startNextBeepAt = 0;
    sequenceIndex = 0;
}

void BlinkSequence::loop() {
    if (millis() >= startNextBeepAt && sequenceIndex < sequence.length()) {
        char action = sequence.charAt(sequenceIndex++);
        long duration = durationFunction(action);
        if (duration > 0) {
            blinker.blink(duration);
        }
        startNextBeepAt = millis() + duration + this->pauseDuration ; 
    }
}

void BlinkSequence::blink(String sequence) {
    this->sequence = sequence;
    sequenceIndex = 0;
    startNextBeepAt = millis();
}