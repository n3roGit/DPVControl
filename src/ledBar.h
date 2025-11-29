#ifndef ledBar_h
#define ledBar_h

#include <Arduino.h> // For uint32_t

const int LedBar2_Num = 10; // (shared) Number of LEDs in the strip


void ledBarSetup();
void knightRiderStartup();
void setBarStandby(bool immediateShow = true);
void setBarSpeed(int num, bool immediateShow = true);
void setBarSpeedCruise(int num, bool immediateShow = true);
void setBarBattery(int num, bool immediateShow = true);
void setBarLeak(bool immediateShow = true);
void setBarPowerBank(bool status, bool immediateShow = true);
void setBarLED(int num, bool immediateShow = true);
void setBarFlasher(bool status, bool immediateShow = true);
void forceRefreshLedBar();
void applyLedBarSettings(); // Apply LED bar settings changes at runtime

// Helper to set bar with colors (now using uint32_t for efficiency/safety)
void setBar(int stripNumber, int numLEDsOn, uint32_t colorOn, int brightnessOn, uint32_t colorOff, int brightnessOff, bool immediateShow = true);

void requestLedBarUpdate(); // Thread-safe request to update LED bar (speed/standby)
void ledBarLoop(); // Main loop handler for LED bar updates

#endif
