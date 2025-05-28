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
    
    // Lamp settings (front light) - Level 0 must always be OFF
    defaultSettings.lampMaxLevels = 5; // OFF + 4 brightness levels
    defaultSettings.lampBrightness[0] = 0;   // Level 0: OFF
    defaultSettings.lampBrightness[1] = 60;  // Level 1: Low brightness
    defaultSettings.lampBrightness[2] = 120; // Level 2: Medium brightness
    defaultSettings.lampBrightness[3] = 180; // Level 3: High brightness
    defaultSettings.lampBrightness[4] = 255; // Level 4: Maximum brightness
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
    log("validateSettings() called - detailed validation");
    
    // Validate speed steps
    if (settings.speedSteps < 1 || settings.speedSteps > 20) {
        String msg = "VALIDATION FAILED: speedSteps " + String(settings.speedSteps) + " not in range 1-20";
        log(msg.c_str());
        return false;
    }
    
    // Validate standby delay
    if (settings.standbyDelaySeconds < 10 || settings.standbyDelaySeconds > 600) {
        String msg = "VALIDATION FAILED: standbyDelaySeconds " + String(settings.standbyDelaySeconds) + " not in range 10-600";
        log(msg.c_str());
        return false;
    }
    
    // Validate battery power
    if (settings.batteryPowerMax < 10 || settings.batteryPowerMax > 100) {
        String msg = "VALIDATION FAILED: batteryPowerMax " + String(settings.batteryPowerMax) + " not in range 10-100";
        log(msg.c_str());
        return false;
    }
    
    // Validate speed percent
    if (settings.minSpeedPercent < 0.1 || settings.minSpeedPercent > 1.0) {
        String msg = "VALIDATION FAILED: minSpeedPercent " + String(settings.minSpeedPercent, 3) + " not in range 0.1-1.0";
        log(msg.c_str());
        return false;
    }
    
    // Validate max RPM
    if (settings.maxSpeedRpm < 1000 || settings.maxSpeedRpm > 50000) {
        String msg = "VALIDATION FAILED: maxSpeedRpm " + String(settings.maxSpeedRpm, 1) + " not in range 1000-50000";
        log(msg.c_str());
        return false;
    }
    
    // Validate timing
    if (settings.speedUpTimeMs < 100 || settings.speedUpTimeMs > 10000) {
        String msg = "VALIDATION FAILED: speedUpTimeMs " + String(settings.speedUpTimeMs) + " not in range 100-10000";
        log(msg.c_str());
        return false;
    }
    if (settings.speedDownTimeMs < 50 || settings.speedDownTimeMs > 5000) {
        String msg = "VALIDATION FAILED: speedDownTimeMs " + String(settings.speedDownTimeMs) + " not in range 50-5000";
        log(msg.c_str());
        return false;
    }
    if (settings.maxTimeOverloadedMs < 1000 || settings.maxTimeOverloadedMs > 30000) {
        String msg = "VALIDATION FAILED: maxTimeOverloadedMs " + String(settings.maxTimeOverloadedMs) + " not in range 1000-30000";
        log(msg.c_str());
        return false;
    }
    
    // Validate jam detection
    if (settings.jamMin < 0.05 || settings.jamMin > 0.5) {
        String msg = "VALIDATION FAILED: jamMin " + String(settings.jamMin, 3) + " not in range 0.05-0.5";
        log(msg.c_str());
        return false;
    }
    if (settings.jamDetectionThreshold < 0.1 || settings.jamDetectionThreshold > 1.0) {
        String msg = "VALIDATION FAILED: jamDetectionThreshold " + String(settings.jamDetectionThreshold, 3) + " not in range 0.1-1.0";
        log(msg.c_str());
        return false;
    }
    
    // Validate cells in series
    if (settings.cellsInSeries < 1 || settings.cellsInSeries > 20) {
        String msg = "VALIDATION FAILED: cellsInSeries " + String(settings.cellsInSeries) + " not in range 1-20";
        log(msg.c_str());
        return false;
    }
    
    // Validate LED settings
    if (settings.ledBarNum < 1 || settings.ledBarNum > 50) {
        String msg = "VALIDATION FAILED: ledBarNum " + String(settings.ledBarNum) + " not in range 1-50";
        log(msg.c_str());
        return false;
    }
    if (settings.ledBarBrightness < 1 || settings.ledBarBrightness > 255) {
        String msg = "VALIDATION FAILED: ledBarBrightness " + String(settings.ledBarBrightness) + " not in range 1-255";
        log(msg.c_str());
        return false;
    }
    if (settings.ledBarBrightnessSecond < 1 || settings.ledBarBrightnessSecond > 255) {
        String msg = "VALIDATION FAILED: ledBarBrightnessSecond " + String(settings.ledBarBrightnessSecond) + " not in range 1-255";
        log(msg.c_str());
        return false;
    }
    if (settings.ledFrequency < 100 || settings.ledFrequency > 10000) {
        String msg = "VALIDATION FAILED: ledFrequency " + String(settings.ledFrequency) + " not in range 100-10000";
        log(msg.c_str());
        return false;
    }
    
    // Validate lamp settings
    if (settings.lampMaxLevels < 2 || settings.lampMaxLevels > 10) {
        String msg = "VALIDATION FAILED: lampMaxLevels " + String(settings.lampMaxLevels) + " not in range 2-10";
        log(msg.c_str());
        return false;
    }
    
    // Log lamp brightness values before validation
    String lampMsg = "Lamp brightness validation - MaxLevels: " + String(settings.lampMaxLevels);
    for (int i = 0; i <= settings.lampMaxLevels; i++) {
        lampMsg += ", Level" + String(i) + ":" + String(settings.lampBrightness[i]);
    }
    log(lampMsg.c_str());
    
    for (int i = 0; i <= settings.lampMaxLevels; i++) {
        if (settings.lampBrightness[i] < 0 || settings.lampBrightness[i] > 255) {
            String msg = "VALIDATION FAILED: lampBrightness[" + String(i) + "] = " + String(settings.lampBrightness[i]) + " not in range 0-255";
            log(msg.c_str());
            return false;
        }
    }
    
    // Validate standby blink
    if (settings.standbyBlinkStartMinutes < 1 || settings.standbyBlinkStartMinutes > 60) {
        String msg = "VALIDATION FAILED: standbyBlinkStartMinutes " + String(settings.standbyBlinkStartMinutes) + " not in range 1-60";
        log(msg.c_str());
        return false;
    }
    if (settings.standbyBlinkDurationSeconds < 1 || settings.standbyBlinkDurationSeconds > 60) {
        String msg = "VALIDATION FAILED: standbyBlinkDurationSeconds " + String(settings.standbyBlinkDurationSeconds) + " not in range 1-60";
        log(msg.c_str());
        return false;
    }
    
    log("Settings validation passed successfully");
    return true;
}

/**
 * Load settings from LittleFS
 */
void loadSettings() {
    log("loadSettings() called");
    
    if (!LittleFS.exists("/dpv_settings.json")) {
        log("No settings file found at /dpv_settings.json, using defaults");
        currentSettings = defaultSettings;
        String defaultMsg = "Default settings loaded - speedSteps: " + String(defaultSettings.speedSteps) + 
                           ", standbyDelay: " + String(defaultSettings.standbyDelaySeconds) +
                           ", beeperEnabled: " + String(defaultSettings.beeperEnabled ? "true" : "false");
        log(defaultMsg.c_str());
        return;
    }
    log("Settings file found at /dpv_settings.json");
    
    File file = LittleFS.open("/dpv_settings.json", "r");
    if (!file) {
        log("ERROR: Failed to open settings file, using defaults");
        currentSettings = defaultSettings;
        return;
    }
    
    size_t fileSize = file.size();
    String fileSizeMsg = "Settings file size: " + String(fileSize) + " bytes";
    log(fileSizeMsg.c_str());
    
    String jsonString = file.readString();
    file.close();
    
    String jsonLengthMsg = "Read JSON string length: " + String(jsonString.length());
    log(jsonLengthMsg.c_str());
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        String errorMsg = "ERROR: Failed to parse settings JSON: " + String(error.c_str()) + ", using defaults";
        log(errorMsg.c_str());
        currentSettings = defaultSettings;
        return;
    }
    log("JSON parsed successfully");
    
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
    log("Validating loaded settings...");
    if (!validateSettings(currentSettings)) {
        log("ERROR: Loaded settings invalid, using defaults");
        currentSettings = defaultSettings;
        return;
    }
    log("Settings validation passed");
    
    // Log final loaded values
    String loadedValues = "Settings loaded - speedSteps: " + String(currentSettings.speedSteps) + 
                         ", standbyDelay: " + String(currentSettings.standbyDelaySeconds) +
                         ", beeperEnabled: " + String(currentSettings.beeperEnabled ? "true" : "false") +
                         ", debugLogging: " + String(currentSettings.debugLoggingEnabled ? "true" : "false");
    log(loadedValues.c_str());
    
    log("Settings loaded successfully from /dpv_settings.json");
}

/**
 * Save settings to LittleFS
 */
void saveSettings() {
    log("saveSettings() called");
    
    // Log current settings values
    String currentValues = "Current settings - speedSteps: " + String(currentSettings.speedSteps) + 
                          ", standbyDelay: " + String(currentSettings.standbyDelaySeconds) +
                          ", beeperEnabled: " + String(currentSettings.beeperEnabled ? "true" : "false") +
                          ", debugLogging: " + String(currentSettings.debugLoggingEnabled ? "true" : "false");
    log(currentValues.c_str());
    
    if (!validateSettings(currentSettings)) {
        log("ERROR: Cannot save invalid settings - validation failed");
        return;
    }
    log("Settings validation passed");
    
    JsonDocument doc;
    
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
    
    // Calculate JSON size
    String jsonString;
    serializeJson(doc, jsonString);
    String jsonSizeMsg = "JSON document size: " + String(jsonString.length()) + " bytes";
    log(jsonSizeMsg.c_str());
    
    log("Opening settings file for writing...");
    File file = LittleFS.open("/dpv_settings.json", "w");
    if (!file) {
        log("ERROR: Failed to open /dpv_settings.json for writing");
        return;
    }
    log("Settings file opened successfully");
    
    size_t bytesWritten = serializeJson(doc, file);
    file.close();
    
    String writeMsg = "Settings file written - " + String(bytesWritten) + " bytes";
    log(writeMsg.c_str());
    
    // Verify the file was written correctly
    if (LittleFS.exists("/dpv_settings.json")) {
        File verifyFile = LittleFS.open("/dpv_settings.json", "r");
        if (verifyFile) {
            size_t fileSize = verifyFile.size();
            verifyFile.close();
            String verifyMsg = "Settings file verified - size: " + String(fileSize) + " bytes";
            log(verifyMsg.c_str());
        } else {
            log("ERROR: Could not open settings file for verification");
        }
    } else {
        log("ERROR: Settings file does not exist after writing");
    }
    
    log("Settings saved successfully to /dpv_settings.json");
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