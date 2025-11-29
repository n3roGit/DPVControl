#ifndef ledLamp_h
#define ledLamp_h

#include "Arduino.h"

void BlinkForLongStandby();
float getLedLampPower();    
int getLedBrightnessPercent(); // Get current LED brightness as percentage (0-100%)
void blinkLED(const String& sequence);
void toggleLED();
void flash();
void setLEDState(int state); // For remote control
void setLampLevel(int level);
void requestSetLampLevel(int level); // Thread-safe request from other cores
bool isLampSequenceActive();
void ledLampSetup();
void ledLampLoop();
void applyLampSettings(); // Apply lamp settings changes at runtime
#endif