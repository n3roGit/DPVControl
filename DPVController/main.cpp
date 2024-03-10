#include "main.h"

#include "constants.h"
#include "Blinker.h" 
#include "BlinkSequence.h" 
#include "log.h"
#include "beep.h"
#include "other.h"
#include "button.h"
#include "datalog.h"
#include "web.h"
#include "dht.h"
#include "leak.h"
#include "motor.h"
#include "ledLamp.h"
#include "ledBar.h"
#include "battery.h"



int loopCount = 0;
int NormalLogOutputIntervall = 1000*10;

/*
The Setup is chaotic. Needs a cleanup
*/
void setup() {

  // Initialize serial communication
  Serial.begin(115200);

  Serial.println("Booting started...!");

  beepSetup();
  buttonSetup();
  motorSetup();
  ledLampSetup();
  ledBarSetup();
  datalogSetup();
  batterySetup();
  webSetup();
  dhtSetup();
  leakSetup();

  // Booting finished
  Serial.println("Booting finished!");
  beep("1");
}

void loop() {
  long loopStart = millis();
  loopCount++;

  buttonLoop();
  motorLoop();
  checkForLeak();
  GetVESCValues();
  logVehicleState();
  FromTimeToTimeExecution();
  beepLoop();
  ledLampLoop();
  datalogLoop();
  webLoop();

  long loopEnd = millis();
  long diff = loopEnd-loopStart;
  if (diff > 30){
    log("Loop took", diff, true);
  }
  
  delay(1);
}

