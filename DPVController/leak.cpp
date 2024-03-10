#include "leak.h"
#include "constants.h"
#include "log.h"
#include "ledBar.h"
#include "Arduino.h"

int leakSensorState = 0;


void leakSetup(){
  pinMode(PIN_LEAK_FRONT, INPUT_PULLUP);
  pinMode(PIN_LEAK_BACK, INPUT_PULLUP);
}

bool getLeakFront(){
    return digitalRead(PIN_LEAK_FRONT) == LOW;
}
bool getLeakBack(){
    return digitalRead(PIN_LEAK_BACK) == LOW;
}

void checkForLeak() {

  if (getLeakFront() || getLeakBack()) {
    leakSensorState = 1;  // There is a leak
    log("leakSensorState", leakSensorState, true);
    setBarLeak();    
  }
}
