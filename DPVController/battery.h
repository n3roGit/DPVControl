#ifndef battery_h
#define battery_h


//Battery
extern int batteryLevel;// 0 to 100% state of charge. 

void batterySetup();

void outputBatteryInfo();

void BatteryLevelAlert();

void updateBatteryLevel(float voltage);
#endif