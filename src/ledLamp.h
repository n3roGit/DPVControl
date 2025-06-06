#ifndef ledLamp_h
#define ledLamp_h

#include "Arduino.h"

void BlinkForLongStandby();
float getLedLampPower();    
void blinkLED(const String& sequence);
void toggleLED();
void flash();
void setLEDState(int state); // For remote control
void ledLampSetup();
void ledLampLoop();
void applyLampSettings(); // Apply lamp settings changes at runtime
#endif