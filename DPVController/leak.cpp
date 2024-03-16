#include "leak.h"
#include "constants.h"
#include "log.h"
#include "ledBar.h"
#include "Arduino.h"

bool hasLeak = false;

int lastNoLeakMs = 0;

void leakSetup()
{
  pinMode(PIN_LEAK_FRONT, INPUT_PULLUP);
  pinMode(PIN_LEAK_BACK, INPUT_PULLUP);
}

bool getLeakFront()
{
  return digitalRead(PIN_LEAK_FRONT) == LOW;
}
bool getLeakBack()
{
  return digitalRead(PIN_LEAK_BACK) == LOW;
}

void checkForLeak()
{

  if (!hasLeak && (getLeakFront() || getLeakBack())){
    if (millis() - lastNoLeakMs >= 1000){
      hasLeak = true; 
      log("hasLeak", hasLeak, true);
      setBarLeak();
    }
  }else{
    lastNoLeakMs = millis();
  }
}
