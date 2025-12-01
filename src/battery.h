#ifndef battery_h
#define battery_h


//Battery
extern int batteryLevel;// 0 to 100% state of charge.

void batterySetup();

void outputBatteryInfo();

void BatteryLevelAlert();

void updateBatteryLevel(float voltage);

// Update the LED display for battery level (call from main loop)
void updateBatteryDisplay(bool forceRefresh = false);

// Function to get the battery voltage
float getBatteryVoltage();

// Get average voltage from measurements
float getAvergageVoltage();

#endif
