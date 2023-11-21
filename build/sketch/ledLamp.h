#line 1 "C:\\Users\\christoph.bubeck\\Documents\\GitHub\\DPVControl\\DPVController\\ledLamp.h"
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