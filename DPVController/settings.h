#ifndef SETTINGS_H
#define SETTINGS_H

#include "Arduino.h"

// Settings structure for all DPV configurations
struct DPVSettings {
    // Motor and speed settings
    int speedSteps;
    int standbyDelaySeconds;
    int batteryPowerMax;
    float minSpeedPercent;
    float maxSpeedRpm;
    int speedUpTimeMs;
    int speedDownTimeMs;
    long maxTimeOverloadedMs;
    
    // Jam detection
    float jamMin;
    float jamDetectionThreshold;
    
    // Battery settings
    int cellsInSeries;
    
    // LED Bar settings
    int ledBarNum;
    int ledBarBrightness;
    int ledBarBrightnessSecond;
    int ledFrequency;
    
    // Lamp settings (front light)
    int lampMaxLevels;
    int lampBrightness[10]; // Support up to 10 brightness levels
    
    // WiFi settings
    char wifiSSID[32];
    char wifiPassword[32];
    
    // Beeper setting
    bool beeperEnabled;
    
    // Standby blink settings
    int standbyBlinkStartMinutes;
    int standbyBlinkDurationSeconds;
};

// Default values based on your constants
extern DPVSettings currentSettings;
extern DPVSettings defaultSettings;

// Function declarations
void initializeSettings();
void loadSettings();
void saveSettings();
void restoreDefaultSettings();
bool validateSettings(const DPVSettings& settings);

// Individual setting getters (for backward compatibility)
int getSpeedSteps();
int getStandbyDelay();
int getBatteryPowerMax();
float getMinSpeedPercent();
float getMaxSpeedRpm();
int getSpeedUpTime();
int getSpeedDownTime();
long getMaxTimeOverloaded();
float getJamMin();
float getJamDetectionThreshold();
int getCellsInSeries();
int getLedBarNum();
int getLedBarBrightness();
int getLedBarBrightnessSecond();
int getLedFrequency();
int getLampMaxLevels();
int getLampBrightness(int level);
const char* getWifiSSID();
const char* getWifiPassword();
bool getBeeperEnabled();
int getStandbyBlinkStart();
int getStandbyBlinkDuration();

#endif // SETTINGS_H 