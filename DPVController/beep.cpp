#include "constants.h"
#include "Blinker.h"
#include "BlinkSequence.h"
#include "log.h"
#include "main.h"
#include "motor.h"

/**
* CONSTANTS
*/
const long SHORT_BEEP_MS = 200;
const long LONG_BEEP_MS = 600;
const long PAUSE_MS = 400;

/**
* VARIABLES
*/
unsigned long lastBeepTime = 0;
unsigned long lastLeakBeepTime = 0;

void turnOnFunction(){digitalWrite(PIN_BEEP, HIGH);}

void turnOffFunction(){digitalWrite(PIN_BEEP, LOW);}

Blinker beepBlinker = Blinker(turnOnFunction, turnOffFunction);


long beepDuration(char c){
   return (c == '1') ? SHORT_BEEP_MS : LONG_BEEP_MS;
}

BlinkSequence beepSequence = BlinkSequence(beepBlinker, beepDuration, PAUSE_MS);


void beepSetup(){
    pinMode(PIN_BEEP, OUTPUT);
}


/**
* Perform a beep for the given time. Works asynchronously. 
*/
void beep(long length_ms){
  log("Beeping for ms", length_ms);
  beepBlinker.blink(length_ms);
}

/**
* Perform a beep for each character in the string with a pause between beeps. 
* a "1" in the string will be a short beep. All others long beeps. 
* Works asynchronously(does not block).
*/
void beep(const String& sequence) {
  if(EnableDebugLog) Serial.println("beepSequence:"+sequence);
  beepSequence.blink(sequence);
}

void beepLoop(){
  beepSequence.loop();
  beepBlinker.loop();
}


void BeepForLeak() {
  if (leakSensorState == 1 && micros() - lastBeepTime >= (10 * 1000 * 1000)) {  // Every 10 seconds
    beep("12121212");                                                              // Here is the desired sequence for the sound
    log("WARNING LEAK", 12121212, true);
    lastLeakBeepTime = micros();  // update the time of the last call
  }
}

