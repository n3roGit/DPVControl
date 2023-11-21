#line 1 "C:\\Users\\christoph.bubeck\\Documents\\GitHub\\DPVControl\\DPVController\\battery.h"
#ifndef battery_h
#define bettery_h


//Battery
extern int batteryLevel;// 0 to 100% state of charge. 

void batterySetup();

void outputBatteryInfo();

void BatteryLevelAlert();

void updateBatteryLevel(float voltage);
#endif