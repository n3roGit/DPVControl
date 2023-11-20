#ifndef ledLamp_h
#define ledLamp_h

#include "Arduino.h"

void BlinkForLongStandby();
float getLedLampPower();    
void blinkLED(const String& sequence);
void toggleLED();
void flash();
void ledLampSetup();
void ledLampLoop();
#endif