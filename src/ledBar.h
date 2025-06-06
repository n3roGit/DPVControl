#ifndef ledBar_h
#define ledBar_h

const int LedBar2_Num = 10; // (shared) Number of LEDs in the strip


void ledBarSetup();
void knightRiderStartup();
void setBarStandby() ;
void setBarSpeed(int num) ;
void setBarSpeedCruise(int num);
void setBarBattery(int num);
void setBarLeak();
void setBarPowerBank(bool status);
void setBarLED(int num);
void setBarFlasher(bool status);
void forceRefreshLedBar();
void applyLedBarSettings(); // Apply LED bar settings changes at runtime


#endif