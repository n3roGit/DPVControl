#include "constants.h"
#include "Blinker.h"
#include "BlinkSequence.h"
#include "log.h"
#include "main.h"
#include "motor.h"
#include <LittleFS.h>

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
bool beeperEnabled = true; // Default enabled

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
  if (!beeperEnabled) return; // Skip if beeper disabled
  log("Beeping for ms", length_ms);
  beepBlinker.blink(length_ms);
}

/**
* Perform a beep for each character in the string with a pause between beeps. 
* a "1" in the string will be a short beep. All others long beeps. 
* Works asynchronously(does not block).
*/
void beep(const String& sequence) {
  if (!beeperEnabled) return; // Skip if beeper disabled
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

/**
* Save beeper settings to SPIFFS
*/
void saveBeeperSettings() {
  File file = LittleFS.open("/beeper_settings.txt", "w");
  if (file) {
    file.println(beeperEnabled ? "1" : "0");
    file.close();
    log("Beeper settings saved");
  } else {
    log("Failed to save beeper settings");
  }
}

/**
* Load beeper settings from SPIFFS
*/
void loadBeeperSettings() {
  if (LittleFS.exists("/beeper_settings.txt")) {
    File file = LittleFS.open("/beeper_settings.txt", "r");
    if (file) {
      String setting = file.readString();
      setting.trim();
      beeperEnabled = (setting == "1");
      file.close();
      String loadMsg = "Beeper settings loaded: " + String(beeperEnabled ? "enabled" : "disabled");
      log(loadMsg.c_str());
    } else {
      log("Failed to load beeper settings");
    }
  } else {
    beeperEnabled = true; // Default enabled
    log("No beeper settings found, using default (enabled)");
  }
}

