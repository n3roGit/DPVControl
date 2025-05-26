#include "settings.h"
#include "log.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

// Settings instances
DPVSettings currentSettings;
DPVSettings defaultSettings;

/**
 * Initialize default settings based on your constants
 */
void initializeDefaultSettings() {
    // Motor and speed settings
    defaultSettings.speedSteps = 10;
    defaultSettings.standbyDelaySeconds = 60;
    defaultSettings.batteryPowerMax = 40;
    defaultSettings.minSpeedPercent = 0.38;
    defaultSettings.maxSpeedRpm = 15800;
    defaultSettings.speedUpTimeMs = 3000;
    defaultSettings.speedDownTimeMs = 500;
    defaultSettings.maxTimeOverloadedMs = 5000;
    
    // Jam detection
    defaultSettings.jamMin = 0.2;
    defaultSettings.jamDetectionThreshold = 0.5;
    
    // Battery settings
    defaultSettings.cellsInSeries = 13;
    
    // LED Bar settings
    defaultSettings.ledBarNum = 10;
    defaultSettings.ledBarBrightness = 15;
    defaultSettings.ledBarBrightnessSecond = 3;
    defaultSettings.ledFrequency = 960;
    
    // Lamp settings (front light) - based on your switch statement
    defaultSettings.lampMaxLevels = 5; // LAMP_OFF to LAMP_MAX (0-4)
    defaultSettings.lampBrightness[0] = 0;   // LAMP_OFF
    defaultSettings.lampBrightness[1] = 20;  // Level 1
    defaultSettings.lampBrightness[2] = 76;  // Level 2
    defaultSettings.lampBrightness[3] = 153; // Level 3
    defaultSettings.lampBrightness[4] = 255; // LAMP_MAX
    // Initialize remaining levels to 0
    for (int i = 5; i < 10; i++) {
        defaultSettings.lampBrightness[i] = 0;
    }
    
    // WiFi settings
    strncpy(defaultSettings.wifiSSID, "DPVControl", sizeof(defaultSettings.wifiSSID));
    strncpy(defaultSettings.wifiPassword, "DPVControl", sizeof(defaultSettings.wifiPassword));
    
    // Beeper setting
    defaultSettings.beeperEnabled = true;
    
    // Standby blink settings
    defaultSettings.standbyBlinkStartMinutes = 15;
    defaultSettings.standbyBlinkDurationSeconds = 10;

    // Debug logging setting
    defaultSettings.debugLoggingEnabled = true;
}

/**
 * Initialize settings system
 */
void initializeSettings() {
    initializeDefaultSettings();
    loadSettings();
    log("Settings system initialized");
}

/**
 * Validate settings for reasonable ranges
 */
bool validateSettings(const DPVSettings& settings) {
    // Validate speed steps
    if (settings.speedSteps < 1 || settings.speedSteps > 20) return false;
    
    // Validate standby delay
    if (settings.standbyDelaySeconds < 10 || settings.standbyDelaySeconds > 600) return false;
    
    // Validate battery power
    if (settings.batteryPowerMax < 10 || settings.batteryPowerMax > 100) return false;
    
    // Validate speed percent
    if (settings.minSpeedPercent < 0.1 || settings.minSpeedPercent > 1.0) return false;
    
    // Validate max RPM
    if (settings.maxSpeedRpm < 1000 || settings.maxSpeedRpm > 50000) return false;
    
    // Validate timing
    if (settings.speedUpTimeMs < 100 || settings.speedUpTimeMs > 10000) return false;
    if (settings.speedDownTimeMs < 50 || settings.speedDownTimeMs > 5000) return false;
    if (settings.maxTimeOverloadedMs < 1000 || settings.maxTimeOverloadedMs > 30000) return false;
    
    // Validate jam detection
    if (settings.jamMin < 0.05 || settings.jamMin > 0.5) return false;
    if (settings.jamDetectionThreshold < 0.1 || settings.jamDetectionThreshold > 1.0) return false;
    
    // Validate cells in series
    if (settings.cellsInSeries < 1 || settings.cellsInSeries > 20) return false;
    
    // Validate LED settings
    if (settings.ledBarNum < 1 || settings.ledBarNum > 50) return false;
    if (settings.ledBarBrightness < 1 || settings.ledBarBrightness > 255) return false;
    if (settings.ledBarBrightnessSecond < 1 || settings.ledBarBrightnessSecond > 255) return false;
    if (settings.ledFrequency < 100 || settings.ledFrequency > 10000) return false;
    
    // Validate lamp settings
    if (settings.lampMaxLevels < 2 || settings.lampMaxLevels > 10) return false;
    for (int i = 0; i < settings.lampMaxLevels; i++) {
        if (settings.lampBrightness[i] < 0 || settings.lampBrightness[i] > 255) return false;
    }
    
    // Validate standby blink
    if (settings.standbyBlinkStartMinutes < 1 || settings.standbyBlinkStartMinutes > 60) return false;
    if (settings.standbyBlinkDurationSeconds < 1 || settings.standbyBlinkDurationSeconds > 60) return false;
    
    return true;
}

/**
 * Load settings from LittleFS
 */
void loadSettings() {
    if (!LittleFS.exists("/dpv_settings.json")) {
        log("No settings file found, using defaults");
        currentSettings = defaultSettings;
        return;
    }
    
    File file = LittleFS.open("/dpv_settings.json", "r");
    if (!file) {
        log("Failed to open settings file, using defaults");
        currentSettings = defaultSettings;
        return;
    }
    
    String jsonString = file.readString();
    file.close();
    
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        log("Failed to parse settings JSON, using defaults");
        currentSettings = defaultSettings;
        return;
    }
    
    // Load settings from JSON
    currentSettings.speedSteps = doc["speedSteps"] | defaultSettings.speedSteps;
    currentSettings.standbyDelaySeconds = doc["standbyDelaySeconds"] | defaultSettings.standbyDelaySeconds;
    currentSettings.batteryPowerMax = doc["batteryPowerMax"] | defaultSettings.batteryPowerMax;
    currentSettings.minSpeedPercent = doc["minSpeedPercent"] | defaultSettings.minSpeedPercent;
    currentSettings.maxSpeedRpm = doc["maxSpeedRpm"] | defaultSettings.maxSpeedRpm;
    currentSettings.speedUpTimeMs = doc["speedUpTimeMs"] | defaultSettings.speedUpTimeMs;
    currentSettings.speedDownTimeMs = doc["speedDownTimeMs"] | defaultSettings.speedDownTimeMs;
    currentSettings.maxTimeOverloadedMs = doc["maxTimeOverloadedMs"] | defaultSettings.maxTimeOverloadedMs;
    
    currentSettings.jamMin = doc["jamMin"] | defaultSettings.jamMin;
    currentSettings.jamDetectionThreshold = doc["jamDetectionThreshold"] | defaultSettings.jamDetectionThreshold;
    
    currentSettings.cellsInSeries = doc["cellsInSeries"] | defaultSettings.cellsInSeries;
    
    currentSettings.ledBarNum = doc["ledBarNum"] | defaultSettings.ledBarNum;
    currentSettings.ledBarBrightness = doc["ledBarBrightness"] | defaultSettings.ledBarBrightness;
    currentSettings.ledBarBrightnessSecond = doc["ledBarBrightnessSecond"] | defaultSettings.ledBarBrightnessSecond;
    currentSettings.ledFrequency = doc["ledFrequency"] | defaultSettings.ledFrequency;
    
    currentSettings.lampMaxLevels = doc["lampMaxLevels"] | defaultSettings.lampMaxLevels;
    for (int i = 0; i < 10; i++) {
        currentSettings.lampBrightness[i] = doc["lampBrightness"][i] | defaultSettings.lampBrightness[i];
    }
    
    strncpy(currentSettings.wifiSSID, doc["wifiSSID"] | defaultSettings.wifiSSID, sizeof(currentSettings.wifiSSID));
    strncpy(currentSettings.wifiPassword, doc["wifiPassword"] | defaultSettings.wifiPassword, sizeof(currentSettings.wifiPassword));
    
    currentSettings.beeperEnabled = doc["beeperEnabled"] | defaultSettings.beeperEnabled;
    currentSettings.debugLoggingEnabled = doc["debugLoggingEnabled"] | defaultSettings.debugLoggingEnabled;
    
    currentSettings.standbyBlinkStartMinutes = doc["standbyBlinkStartMinutes"] | defaultSettings.standbyBlinkStartMinutes;
    currentSettings.standbyBlinkDurationSeconds = doc["standbyBlinkDurationSeconds"] | defaultSettings.standbyBlinkDurationSeconds;
    
    // Validate loaded settings
    if (!validateSettings(currentSettings)) {
        log("Loaded settings invalid, using defaults");
        currentSettings = defaultSettings;
        return;
    }
    
    log("Settings loaded successfully");
}

/**
 * Save settings to LittleFS
 */
void saveSettings() {
    if (!validateSettings(currentSettings)) {
        log("Cannot save invalid settings");
        return;
    }
    
    DynamicJsonDocument doc(2048);
    
    // Save all settings to JSON
    doc["speedSteps"] = currentSettings.speedSteps;
    doc["standbyDelaySeconds"] = currentSettings.standbyDelaySeconds;
    doc["batteryPowerMax"] = currentSettings.batteryPowerMax;
    doc["minSpeedPercent"] = currentSettings.minSpeedPercent;
    doc["maxSpeedRpm"] = currentSettings.maxSpeedRpm;
    doc["speedUpTimeMs"] = currentSettings.speedUpTimeMs;
    doc["speedDownTimeMs"] = currentSettings.speedDownTimeMs;
    doc["maxTimeOverloadedMs"] = currentSettings.maxTimeOverloadedMs;
    
    doc["jamMin"] = currentSettings.jamMin;
    doc["jamDetectionThreshold"] = currentSettings.jamDetectionThreshold;
    
    doc["cellsInSeries"] = currentSettings.cellsInSeries;
    
    doc["ledBarNum"] = currentSettings.ledBarNum;
    doc["ledBarBrightness"] = currentSettings.ledBarBrightness;
    doc["ledBarBrightnessSecond"] = currentSettings.ledBarBrightnessSecond;
    doc["ledFrequency"] = currentSettings.ledFrequency;
    
    doc["lampMaxLevels"] = currentSettings.lampMaxLevels;
    for (int i = 0; i < 10; i++) {
        doc["lampBrightness"][i] = currentSettings.lampBrightness[i];
    }
    
    doc["wifiSSID"] = currentSettings.wifiSSID;
    doc["wifiPassword"] = currentSettings.wifiPassword;
    
    doc["beeperEnabled"] = currentSettings.beeperEnabled;
    doc["debugLoggingEnabled"] = currentSettings.debugLoggingEnabled;
    
    doc["standbyBlinkStartMinutes"] = currentSettings.standbyBlinkStartMinutes;
    doc["standbyBlinkDurationSeconds"] = currentSettings.standbyBlinkDurationSeconds;
    
    File file = LittleFS.open("/dpv_settings.json", "w");
    if (!file) {
        log("Failed to open settings file for writing");
        return;
    }
    
    serializeJson(doc, file);
    file.close();
    
    log("Settings saved successfully");
}

/**
 * Restore default settings
 */
void restoreDefaultSettings() {
    currentSettings = defaultSettings;
    saveSettings();
    log("Default settings restored");
}

// Getter functions for backward compatibility
int getSpeedSteps() { return currentSettings.speedSteps; }
int getStandbyDelay() { return currentSettings.standbyDelaySeconds; }
int getBatteryPowerMax() { return currentSettings.batteryPowerMax; }
float getMinSpeedPercent() { return currentSettings.minSpeedPercent; }
float getMaxSpeedRpm() { return currentSettings.maxSpeedRpm; }
int getSpeedUpTime() { return currentSettings.speedUpTimeMs; }
int getSpeedDownTime() { return currentSettings.speedDownTimeMs; }
long getMaxTimeOverloaded() { return currentSettings.maxTimeOverloadedMs; }
float getJamMin() { return currentSettings.jamMin; }
float getJamDetectionThreshold() { return currentSettings.jamDetectionThreshold; }
int getCellsInSeries() { return currentSettings.cellsInSeries; }
int getLedBarNum() { return currentSettings.ledBarNum; }
int getLedBarBrightness() { return currentSettings.ledBarBrightness; }
int getLedBarBrightnessSecond() { return currentSettings.ledBarBrightnessSecond; }
int getLedFrequency() { return currentSettings.ledFrequency; }
int getLampMaxLevels() { return currentSettings.lampMaxLevels; }
int getLampBrightness(int level) { 
    if (level < 0 || level >= 10) return 0;
    return currentSettings.lampBrightness[level]; 
}
const char* getWifiSSID() { return currentSettings.wifiSSID; }
const char* getWifiPassword() { return currentSettings.wifiPassword; }
bool getBeeperEnabled() { return currentSettings.beeperEnabled; }
bool getDebugLoggingEnabled() { return currentSettings.debugLoggingEnabled; }
int getStandbyBlinkStart() { return currentSettings.standbyBlinkStartMinutes; }
int getStandbyBlinkDuration() { return currentSettings.standbyBlinkDurationSeconds; } 