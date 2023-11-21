#line 1 "C:\\Users\\christoph.bubeck\\Documents\\GitHub\\DPVControl\\DPVController\\BlinkSequence.h"
#ifndef BlinkSequence_h
#define BlinkSequence_h

#include "Arduino.h"
#include "Blinker.h"

/**
* Allows us to do a sequence of blinks of different length. 
*/
class BlinkSequence {
public:
    BlinkSequence(
      Blinker &blinker, 
      long (*durationFunction)(char),
      long pauseDuration);
    void loop();
    void blink(String sequence);

private:
    Blinker &blinker;
    String sequence;
    long startNextBeepAt;
    long pauseDuration;
    unsigned int sequenceIndex;
    long (*durationFunction)(char);
};

#endif
