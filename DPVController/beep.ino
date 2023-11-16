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
//Marker values.
const int NEVER = -1;
String BEEP_NONE = "NONE"; 

/**
* GLOBAL VARIABLES
*/
long stopBeepAt = NEVER; //Millisecond timestamp at which we stop beeping. 
long startNextBeepAt = NEVER; //Millisecond timestamp at which we start working on 
//the next character in a sequence. 
String beepSequence = BEEP_NONE;
int beepPos = 0;//Current index within beepSequence.

void turnOnFunction(){digitalWrite(PIN_BEEP, HIGH);}

void turnOffFunction(){digitalWrite(PIN_BEEP, LOW);}

Blinker beepBlinker = Blinker(turnOnFunction, turnOffFunction);

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
  if (!beepSequence.equals(BEEP_NONE)){
    Serial.println("Warning! overriding beep "+beepSequence+ " with "+sequence);
  }
  beepSequence = sequence;
  beepPos = 0;
  startNextBeepAt = millis();
}

void beepLoop(){

  //Process the current beepsequence. 
  if (!beepSequence.equals(BEEP_NONE)){
    if (startNextBeepAt != NEVER && startNextBeepAt <= millis()){
      char c = beepSequence[beepPos++];
      int toneDuration = (c == '1') ? SHORT_BEEP_MS : LONG_BEEP_MS;
      beep(toneDuration);
      if (beepPos == beepSequence.length()){
        //Reached end of string. we are done. 
        beepSequence = BEEP_NONE;
        startNextBeepAt = NEVER;
        Serial.println("Done beeping sequence.");
      }else{
        //Schedule next run to continue after a pause.
        startNextBeepAt = millis()+toneDuration+PAUSE_MS;
      }
    }
  }

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
