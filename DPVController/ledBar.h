#ifndef ledBar_h
#define ledBar_h

//FIXME: why does the battery need that?
const int LedBar2_Num = 10; // (shared) Number of LEDs in the strip


void ledBarSetup();
void setBarStandby() ;
void setBarSpeed(int num) ;
void setBarBattery(int num);
void setBarLeak();
void setBarPowerBank(bool status);
void setBarLED(int num);
void setBarFlasher(bool status);


#endif