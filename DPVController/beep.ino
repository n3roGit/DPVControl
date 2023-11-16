/**
* Code for making beeps. Surprisingly hard, because we avoid
* using delay() so that we do not block all execution while
* we beep. We achieve this by using running a loop that checks the current time with
* a timestamp to determine if we need to stop beeping or continue the next beep
* in a series of beeps. 
*
* Since we use global variables for this, we can only do one beep or beepSeuence at a time. Extra calls will override the current one. 
* We could instead queue them using a ... queue.
*/


/**
* CONSTANTS
*/
const long SHORT_BEEP_MS = 200;
const long LONG_BEEP_MS = 600;
const long PAUSE_MS = 400;

/**
* GLOBAL VARIABLES
*/

void turnOnFunction(){digitalWrite(PIN_BEEP, HIGH);}

void turnOffFunction(){digitalWrite(PIN_BEEP, LOW);}

Blinker beepBlinker = Blinker(turnOnFunction, turnOffFunction);


long beepDuration(char c){
   return (c == '1') ? SHORT_BEEP_MS : LONG_BEEP_MS;
}

BlinkSequence beepSequence = BlinkSequence(beepBlinker, beepDuration, PAUSE_MS);


/**
* Perform a beep for the given time. Works asynchronously. 
*/
void beep(long length_ms){
  log("Beeping for ms", length_ms, EnableDebugLog);
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
    beep("22222");                                                              // Here is the desired sequence for the sound
    log("WARNING LEAK", 22222, true);
    lastLeakBeepTime = micros();  // update the time of the last call
  }
}
void BeepForStandby() {
  if (motorState == MOTOR_STANDBY && micros() - lastStandbyBeepTime >= (1 * 60 * 1000000)) {
    beep("1");  // Hier die gewünschte Sequenz für den Ton
    log("still in standby", 1, true);
    lastStandbyBeepTime = micros();  // Update the time of the last call
  }
}
