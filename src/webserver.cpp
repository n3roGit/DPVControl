// Copyright 2025 BubTec
#include "webserver.h"
#include "log.h"
#include "data_upload.h"
#include "datalog.h"  // Include datalogger headers
#include "constants.h" // For PIN definitions
#include "beep.h" // For beeper settings
#include "settings.h" // For DPV settings system
#include "motor.h" // For motor control functions
#include "ledLamp.h" // For lamp control functions
#include "ledBar.h" // For LED bar functions
#include <LittleFS.h> // Add missing LittleFS include
#include <ArduinoJson.h> // For JSON parsing
#include "button.h"
#include <algorithm>  // For min() and max()
#include <WiFi.h>
#include <WebServer.h>
#include "main.h" // For dhtSensor global variable
#include "battery.h" // For batteryLevel global variable
#include "embedded_webserver.h" // For embedded file serving

// External variables
extern int LED_State; // From ledLamp.cpp
extern int currentMotorStep; // From motor.cpp
extern MotorState motorState; // From motor.cpp
extern unsigned long lastActionTime; // From main.cpp
extern bool remoteControlActive; // From motor.cpp - for remote control mode

// External function declarations
extern void wakeUp(); // From motor.cpp
extern void setBarSpeed(int speed); // From ledBar.cpp
extern void setLEDState(int state); // From ledLamp.cpp
extern void setBarLED(int level); // From ledBar.cpp
extern int getTotalDataPoints(String timeRange); // From datalog.cpp
extern LogdataRow* getLatestDataPoints(int count, String timeRange); // From datalog.cpp
extern String* listSessionFiles(int* count); // From datalog.cpp
extern String getCurrentSessionFile(); // From datalog.cpp
extern unsigned long getTotalUptime(); // From datalog.cpp

// Task handle for the webserver task
TaskHandle_t webserverTaskHandle = NULL;

// DNS Server for captive portal
const byte DNS_PORT = 53;
IPAddress apIP(4, 3, 2, 1);
DNSServer dnsServer;
WiFiServer server(80);

// Flag to check if SPIFFS is mounted
bool spiffsInitialized = false;

/**
 * Generate JSON list of all available sessions sorted with newest first
 * Groups session splits together for better organization
 */
String generateSessionListJson() {
    log("generateSessionListJson called");
    int count;
    String* sessions = listSessionFiles(&count);
    String countMsg = "Found " + String(count) + " session files";
    log(countMsg.c_str());
    
    String currentSession = getCurrentSessionFile();
    String currentMsg = "Current session: " + currentSession;
    log(currentMsg.c_str());
    // Extract filename from full path
    if (currentSession.startsWith("/datalog/")) {
        currentSession = currentSession.substring(9); // Remove "/datalog/"
    }
    
    // Sort sessions by filename (newest first for 4-digit numbering)
    // Bubble sort for simplicity
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (sessions[j] < sessions[j + 1]) { // Reverse order for newest first
                String temp = sessions[j];
                sessions[j] = sessions[j + 1];
                sessions[j + 1] = temp;
            }
        }
    }
    
    String json = "[";
    for (int i = 0; i < count; i++) {
        if (i > 0) json += ",";
        
        // Extract display name for session splits
        String displayName = sessions[i];
        String sessionNumber = "";
        String splitInfo = "";
        
        // Parse session filename like "session_0001.bin" or "session_0001-02.bin"
        if (displayName.startsWith("session_") && displayName.endsWith(".bin")) {
            String numberPart = displayName.substring(8); // Remove "session_"
            numberPart = numberPart.substring(0, numberPart.length() - 4); // Remove ".bin"
            
            int dashPos = numberPart.indexOf('-');
            if (dashPos != -1) {
                sessionNumber = numberPart.substring(0, dashPos);
                splitInfo = numberPart.substring(dashPos + 1);
                displayName = "Session " + sessionNumber + " (Part " + splitInfo + ")";
            } else {
                sessionNumber = numberPart;
                displayName = "Session " + sessionNumber;
            }
        }
        
        // Add current session indicator
        bool isCurrent = (sessions[i] == currentSession);
        if (isCurrent) {
            displayName += " (Current)";
        }
        
        // Create an object with filename, display name and current flag
        json += "{";
        json += "\"filename\":\"" + sessions[i] + "\",";
        json += "\"displayName\":\"" + displayName + "\",";
        json += "\"sessionNumber\":\"" + sessionNumber + "\",";
        json += "\"splitInfo\":\"" + splitInfo + "\",";
        json += "\"isCurrent\":" + String(isCurrent ? "true" : "false");
        json += "}";
    }
    json += "]";
    
    return json;
}

/**
 * Generate JSON data for a specific session with delta compression support and interpolation
 */
String generateSessionDataJson(String sessionFile) {
    // Ensure we have the full path
    String fullPath = sessionFile;
    if (!sessionFile.startsWith("/datalog/")) {
        fullPath = "/datalog/" + sessionFile;
    }
    
    String pathMsg = "Session data request - File: " + sessionFile + ", Full path: " + fullPath;
    log(pathMsg.c_str());
    
    if (!LittleFS.exists(fullPath)) {
        log("Session file does not exist!");
        return "{\"data\":[],\"meta\":{\"error\":\"File not found\"}}";
    }
    
    File file = LittleFS.open(fullPath, "r");
    if (!file) {
        log("Cannot open session file!");
        return "{\"data\":[],\"meta\":{\"error\":\"Cannot open file\"}}";
    }
    
    // Get file size and calculate total datapoints
    size_t fileSize = file.size();
    int totalDatapoints = fileSize / sizeof(LogdataRow);
    
    // Read first and last datapoint for metadata
    LogdataRow firstDataPoint, lastDataPoint;
    long realSessionStartMs = 0;
    long realSessionEndMs = 0;
    int realSessionDurationSeconds = 0;
    
    if (totalDatapoints > 0) {
        file.seek(0);
        file.read((uint8_t*)&firstDataPoint, sizeof(LogdataRow));
        realSessionStartMs = firstDataPoint.timestamp;
        
        file.seek((totalDatapoints - 1) * sizeof(LogdataRow));
        file.read((uint8_t*)&lastDataPoint, sizeof(LogdataRow));
        realSessionEndMs = lastDataPoint.timestamp;
        
        realSessionDurationSeconds = (realSessionEndMs - realSessionStartMs) / 1000;
    }
    
    // Calculate optimal number of points based on duration
    const int maxPoints = 1000; // Increased from 100 to support larger sessions (memory allows up to 1000 points)
    int targetPoints = maxPoints;
    
    if (realSessionDurationSeconds > 0) {
        // One point per 3 seconds for better resolution on longer sessions
        targetPoints = min(maxPoints, realSessionDurationSeconds / 3);
        targetPoints = max(10, targetPoints); // At least 10 points
    }
    
    // Calculate skip interval
    int skipInterval = totalDatapoints > targetPoints ? totalDatapoints / targetPoints : 1;
    
    // Create JSON document with fixed size
    JsonDocument doc;
    
    // Add metadata
    JsonObject meta = doc["meta"].to<JsonObject>();
    meta["realStartTimestamp"] = realSessionStartMs;
    meta["realEndTimestamp"] = realSessionEndMs;
    meta["realDurationSeconds"] = realSessionDurationSeconds;
    meta["totalDatapoints"] = totalDatapoints;
    meta["chartDatapoints"] = targetPoints;
    meta["skipInterval"] = skipInterval;
    
    // Add data array
    JsonArray data = doc["data"].to<JsonArray>();
    
    // Read and add datapoints
    for (int i = 0; i < totalDatapoints && data.size() < targetPoints; i += skipInterval) {
        LogdataRow row;
        file.seek(i * sizeof(LogdataRow));
        if (file.read((uint8_t*)&row, sizeof(LogdataRow)) == sizeof(LogdataRow)) {
            // Filter out corrupted data points with invalid boolean values
            if (row.batteryVoltage > 20.0 && row.batteryVoltage < 100.0 && // Valid battery voltage range
                (row.leftButton == 0 || row.leftButton == 1) &&             // Clean boolean values
                (row.rightButton == 0 || row.rightButton == 1) &&
                (row.beeperEnabled == 0 || row.beeperEnabled == 1) &&
                (row.beeperActive == 0 || row.beeperActive == 1)) {
                
                JsonObject point = data.add<JsonObject>();
                point["timestamp"] = row.timestamp;
                point["tempMotor"] = row.tempMotor;
                point["tempMosfet"] = row.tempMosfet;
                point["batteryVoltage"] = row.batteryVoltage;
                point["current"] = row.current;
                point["avgMotorCurrent"] = row.avgMotorCurrent;
                point["rpm"] = row.erpm;
                point["dutyCycle"] = row.dutyCycle;
                point["temperature"] = row.temperature;
                point["humidity"] = row.humidity;
                point["batteryLevel"] = row.batteryLevel;
                point["leakSensorState"] = row.leakSensorState;
                point["ledBrightness"] = row.ledBrightness;
                point["leftButton"] = row.leftButton;
                point["rightButton"] = row.rightButton;
                point["beeperEnabled"] = row.beeperEnabled;
                point["beeperActive"] = row.beeperActive;
                point["totalUptime"] = row.totalUptime;
            }
        }
    }
    
    file.close();
    
    // Serialize to string
    String json;
    serializeJson(doc, json);
    
    String resultMsg = "Session data JSON generated - Length: " + String(json.length()) + ", Data points: " + String(data.size());
    log(resultMsg.c_str());
    
    // Log first 100 characters for debugging
    if (json.length() > 100) {
        String preview = "JSON preview: " + json.substring(0, 100) + "...";
        log(preview.c_str());
    }
    
    return json;
}

// HTML files are now served from LittleFS data directory instead of embedded code

// Helper function to send a HTTP response
void sendHttpResponse(WiFiClient client, int statusCode, const char* contentType, const char* content) {
    client.print("HTTP/1.1 ");
    client.print(statusCode);
    client.print(" ");
    
    // Status message based on code
    switch(statusCode) {
        case 200: client.println("OK"); break;
        case 302: client.println("Found"); break;
        case 404: client.println("Not Found"); break;
        default: client.println("OK");
    }
    
    client.print("Content-Type: ");
    client.println(contentType);
    
    if (statusCode == 302) {
        client.print("Location: http://");
        client.println(apIP.toString());
        client.println("Cache-Control: no-cache, no-store, must-revalidate");
        client.println("Pragma: no-cache");
        client.println("Expires: -1");
    }
    
    client.print("Content-Length: ");
    client.println(strlen(content));
    client.println("Connection: close");
    client.println();
    client.println(content);
}

// Helper function to load file from embedded storage or SPIFFS fallback
bool loadFromEmbeddedOrSPIFFS(WiFiClient client, String path) {
    // First try to load from embedded files
    if (serveEmbeddedFile(client, path.c_str())) {
        return true;
    }
    
    // Fallback to SPIFFS for backward compatibility
    log("Embedded file not found, trying SPIFFS fallback");
    return loadFromSPIFFS(client, path);
}

// Helper function to load file from SPIFFS and send to client (legacy function)
bool loadFromSPIFFS(WiFiClient client, String path) {
    String dataType = "text/plain";
    
    // Set the correct dataType based on file extension
    if (path.endsWith(".html")) dataType = "text/html";
    else if (path.endsWith(".css")) dataType = "text/css";
    else if (path.endsWith(".js")) dataType = "application/javascript";
    else if (path.endsWith(".png")) dataType = "image/png";
    else if (path.endsWith(".jpg")) dataType = "image/jpeg";
    else if (path.endsWith(".ico")) dataType = "image/x-icon";
    
    // Open the file
    File dataFile = LittleFS.open(path.c_str(), "r");
    
    if (!dataFile) {
        log("Failed to open file");
        return false;
    }
    
    // HTTP response header
    client.println("HTTP/1.1 200 OK");
    client.print("Content-Type: ");
    client.println(dataType);
    client.println("Connection: close");
    client.println();
    
    // Stream file to client
    byte buffer[64];
    int bytesRead;
    
    while ((bytesRead = dataFile.read(buffer, sizeof(buffer))) > 0) {
        client.write(buffer, bytesRead);
    }
    
    // Close the file
    dataFile.close();
    return true;
}

// Helper function to generate JSON data from datalogger data
String generateDataLoggerJson(int count, String timeRange = "recent") {
    log("generateDataLoggerJson called");
    String countMsg = "Requested count: " + String(count) + ", range: " + timeRange + ", available: " + String(getTotalDataPoints(timeRange));
    log(countMsg.c_str());
    
    LogdataRow* dataPoints = getLatestDataPoints(count, timeRange);
    
    // If no data available, return empty array
    if (!dataPoints || getTotalDataPoints(timeRange) == 0) {
        log("No data available, returning empty array");
        return "[]";
    }
    
    log("Building JSON from real data");
    String json = "[";
    int actualCount = count < getTotalDataPoints(timeRange) ? count : getTotalDataPoints(timeRange);
    
    String actualCountMsg = "Building JSON with " + String(actualCount) + " data points from " + timeRange + " range";
    log(actualCountMsg.c_str());
    
    for (int i = 0; i < actualCount; i++) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"timestamp\":" + String(dataPoints[i].timestamp) + ",";
        json += "\"tempMotor\":" + String(dataPoints[i].tempMotor) + ",";
        json += "\"tempMosfet\":" + String(dataPoints[i].tempMosfet) + ",";
        json += "\"batteryVoltage\":" + String(dataPoints[i].batteryVoltage) + ",";
        json += "\"current\":" + String(dataPoints[i].current) + ",";
        json += "\"avgMotorCurrent\":" + String(dataPoints[i].avgMotorCurrent) + ",";
        json += "\"rpm\":" + String(dataPoints[i].erpm) + ",";
        json += "\"dutyCycle\":" + String(dataPoints[i].dutyCycle) + ",";
        json += "\"temperature\":" + String(dataPoints[i].temperature) + ",";
        json += "\"humidity\":" + String(dataPoints[i].humidity) + ",";
        json += "\"batteryLevel\":" + String(dataPoints[i].batteryLevel) + ",";
        json += "\"leakSensorState\":" + String(dataPoints[i].leakSensorState) + ",";
        json += "\"ledBrightness\":" + String(dataPoints[i].ledBrightness) + ",";
        json += "\"leftButton\":" + String(dataPoints[i].leftButton) + ",";
        json += "\"rightButton\":" + String(dataPoints[i].rightButton) + ",";
                        json += "\"beeperEnabled\":" + String(dataPoints[i].beeperEnabled) + ",";
                json += "\"beeperActive\":" + String(dataPoints[i].beeperActive) + ",";
        json += "\"totalUptime\":" + String(dataPoints[i].totalUptime);
        json += "}";
    }
    json += "]";
    
    String jsonLengthMsg = "Generated JSON length: " + String(json.length());
    log(jsonLengthMsg.c_str());
    
    // Debug: Show first part of JSON
    if (json.length() > 100) {
        String jsonPreview = "JSON preview: " + json.substring(0, 100) + "...";
        log(jsonPreview.c_str());
    } else {
        String jsonFull = "JSON full: " + json;
        log(jsonFull.c_str());
    }
    
    return json;
}

/**
 * Generate JSON for complete trip log from LittleFS file
 */
String generateFullTripLogJson() {
    log("generateFullTripLogJson called");
    
    if (!LittleFS.exists("/trip_log.bin")) {
        log("No trip log file found");
        return "";
    }
    
    File tripFile = LittleFS.open("/trip_log.bin", "r");
    if (!tripFile) {
        log("Failed to open trip log file");
        return "";
    }
    
    size_t fileSize = tripFile.size();
    size_t dataPointCount = fileSize / sizeof(LogdataRow);
    
    String countMsg = "Trip log contains " + String(dataPointCount) + " data points (" + String(fileSize) + " bytes)";
    log(countMsg.c_str());
    
    if (dataPointCount == 0) {
        tripFile.close();
        return "";
    }
    
    // CSV Header
    String csv = "Timestamp,Motor Temperature (degC),MOSFET Temperature (degC),Battery Voltage (V),Input Current (A),Motor Current (A),RPM,Duty Cycle (%),Ambient Temperature (degC),Humidity (%),Battery Level (%),Leak Sensor State,LED Brightness (%),Left Button,Right Button,Beeper Enabled,Total Uptime (s)\r\n";
    
    LogdataRow dataPoint;
    
    // Read and convert each data point
    for (size_t i = 0; i < dataPointCount; i++) {
        size_t bytesRead = tripFile.read((uint8_t*)&dataPoint, sizeof(LogdataRow));
        
        if (bytesRead != sizeof(LogdataRow)) {
            String errorMsg = "Error reading data point " + String(i) + ", bytes read: " + String(bytesRead);
            log(errorMsg.c_str());
            break;
        }
        
        // Convert timestamp to ISO format
        time_t timestamp = dataPoint.timestamp / 1000; // Convert to seconds
        struct tm* timeinfo = gmtime(&timestamp);
        char timeStr[30];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%dT%H:%M:%S.000Z", timeinfo);
        
        // Add data row
        csv += String(timeStr) + ",";
        csv += String(dataPoint.tempMotor) + ",";
        csv += String(dataPoint.tempMosfet) + ",";
        csv += String(dataPoint.batteryVoltage) + ",";
        csv += String(dataPoint.current) + ",";
        csv += String(dataPoint.avgMotorCurrent) + ",";
        csv += String(dataPoint.erpm) + ",";
        csv += String(dataPoint.dutyCycle) + ",";
        csv += String(dataPoint.temperature) + ",";
        csv += String(dataPoint.humidity) + ",";
        csv += String(dataPoint.batteryLevel) + ",";
        csv += String(dataPoint.leakSensorState) + ",";
        csv += String(dataPoint.ledBrightness) + ",";
        csv += String(dataPoint.leftButton) + ",";
        csv += String(dataPoint.rightButton) + ",";
                    csv += String(dataPoint.beeperEnabled) + ",";
            csv += String(dataPoint.beeperActive) + ",";
        csv += String(dataPoint.totalUptime) + "\r\n";
        
        // Prevent memory overflow for very large files
        if (csv.length() > 50000) { // Limit to ~50KB
            String limitMsg = "CSV size limit reached at " + String(i+1) + " points, truncating";
            log(limitMsg.c_str());
            break;
        }
    }
    
    tripFile.close();
    
    String resultMsg = "Generated full trip log CSV, length: " + String(csv.length()) + " for " + String(dataPointCount) + " points";
    log(resultMsg.c_str());
    
    return csv;
}

/**
 * Generate JSON for current DPV settings
 */
String generateSettingsJson() {
    log("generateSettingsJson called");
    
    JsonDocument doc;
    
    // Motor and speed settings
    doc["speedSteps"] = currentSettings.speedSteps;
    doc["standbyDelaySeconds"] = currentSettings.standbyDelaySeconds;
    doc["batteryPowerMax"] = currentSettings.batteryPowerMax;
    doc["minSpeedPercent"] = currentSettings.minSpeedPercent;
    doc["maxSpeedRpm"] = currentSettings.maxSpeedRpm;
    doc["speedUpTimeMs"] = currentSettings.speedUpTimeMs;
    doc["speedDownTimeMs"] = currentSettings.speedDownTimeMs;
    doc["maxTimeOverloadedMs"] = currentSettings.maxTimeOverloadedMs;
    
    // Jam detection
    doc["jamMin"] = currentSettings.jamMin;
    doc["jamDetectionThreshold"] = currentSettings.jamDetectionThreshold;
    
    // Battery settings
    doc["cellsInSeries"] = currentSettings.cellsInSeries;
    
    // LED Bar settings
    doc["ledBarNum"] = currentSettings.ledBarNum;
    doc["ledBarBrightness"] = currentSettings.ledBarBrightness;
    doc["ledBarBrightnessSecond"] = currentSettings.ledBarBrightnessSecond;
    doc["ledFrequency"] = currentSettings.ledFrequency;
    
    // Lamp settings
    doc["lampMaxLevels"] = currentSettings.lampMaxLevels;
    JsonArray lampBrightness = doc["lampBrightness"].to<JsonArray>();
    for (int i = 0; i < 10; i++) {
        lampBrightness.add(currentSettings.lampBrightness[i]);
    }
    
    // WiFi settings
    doc["wifiSSID"] = currentSettings.wifiSSID;
    doc["wifiPassword"] = currentSettings.wifiPassword;
    
    // System settings
    doc["beeperEnabled"] = currentSettings.beeperEnabled;
    doc["debugLoggingEnabled"] = currentSettings.debugLoggingEnabled;
    doc["standbyBlinkStartMinutes"] = currentSettings.standbyBlinkStartMinutes;
    doc["standbyBlinkDurationSeconds"] = currentSettings.standbyBlinkDurationSeconds;
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    String jsonMsg = "Generated settings JSON, length: " + String(jsonString.length());
    log(jsonMsg.c_str());
    
    return jsonString;
}

/**
 * Update settings from JSON string
 */
bool updateSettingsFromJson(const String& jsonString) {
    log("updateSettingsFromJson called");
    String logMsg = "JSON length: " + String(jsonString.length());
    log(logMsg.c_str());
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        String errorMsg = "Failed to parse settings JSON: " + String(error.c_str());
        log(errorMsg.c_str());
        return false;
    }
    
    log("JSON parsed successfully");
    
    // Create temporary settings structure
    DPVSettings newSettings = currentSettings;
    
    // Log current values before update
    String currentMsg = "Current speedSteps: " + String(currentSettings.speedSteps) + 
                       ", standbyDelay: " + String(currentSettings.standbyDelaySeconds) +
                       ", beeperEnabled: " + String(currentSettings.beeperEnabled ? "true" : "false");
    log(currentMsg.c_str());
    
    // Update settings from JSON with detailed logging
    int updatedFields = 0;
    if (doc["speedSteps"].is<int>()) {
        int oldVal = newSettings.speedSteps;
        newSettings.speedSteps = doc["speedSteps"];
        String msg = "Updated speedSteps: " + String(oldVal) + " -> " + String(newSettings.speedSteps);
        log(msg.c_str());
        updatedFields++;
    }
    if (doc["standbyDelaySeconds"].is<int>()) {
        int oldVal = newSettings.standbyDelaySeconds;
        newSettings.standbyDelaySeconds = doc["standbyDelaySeconds"];
        String msg = "Updated standbyDelaySeconds: " + String(oldVal) + " -> " + String(newSettings.standbyDelaySeconds);
        log(msg.c_str());
        updatedFields++;
    }
    if (doc["batteryPowerMax"].is<int>()) {
        int oldVal = newSettings.batteryPowerMax;
        newSettings.batteryPowerMax = doc["batteryPowerMax"];
        String msg = "Updated batteryPowerMax: " + String(oldVal) + " -> " + String(newSettings.batteryPowerMax);
        log(msg.c_str());
        updatedFields++;
    }
    if (doc["minSpeedPercent"].is<float>()) {
        float oldVal = newSettings.minSpeedPercent;
        newSettings.minSpeedPercent = doc["minSpeedPercent"];
        String msg = "Updated minSpeedPercent: " + String(oldVal, 3) + " -> " + String(newSettings.minSpeedPercent, 3);
        log(msg.c_str());
        updatedFields++;
    }
    if (doc["maxSpeedRpm"].is<float>()) {
        float oldVal = newSettings.maxSpeedRpm;
        newSettings.maxSpeedRpm = doc["maxSpeedRpm"];
        String msg = "Updated maxSpeedRpm: " + String(oldVal, 1) + " -> " + String(newSettings.maxSpeedRpm, 1);
        log(msg.c_str());
        updatedFields++;
    }
    if (doc["speedUpTimeMs"].is<int>()) {
        int oldVal = newSettings.speedUpTimeMs;
        newSettings.speedUpTimeMs = doc["speedUpTimeMs"];
        String msg = "Updated speedUpTimeMs: " + String(oldVal) + " -> " + String(newSettings.speedUpTimeMs);
        log(msg.c_str());
        updatedFields++;
    }
    if (doc["speedDownTimeMs"].is<int>()) {
        int oldVal = newSettings.speedDownTimeMs;
        newSettings.speedDownTimeMs = doc["speedDownTimeMs"];
        String msg = "Updated speedDownTimeMs: " + String(oldVal) + " -> " + String(newSettings.speedDownTimeMs);
        log(msg.c_str());
        updatedFields++;
    }
    if (doc["maxTimeOverloadedMs"].is<long>()) {
        long oldVal = newSettings.maxTimeOverloadedMs;
        newSettings.maxTimeOverloadedMs = doc["maxTimeOverloadedMs"];
        String msg = "Updated maxTimeOverloadedMs: " + String(oldVal) + " -> " + String(newSettings.maxTimeOverloadedMs);
        log(msg.c_str());
        updatedFields++;
    }
    
    if (doc["jamMin"].is<float>()) newSettings.jamMin = doc["jamMin"];
    if (doc["jamDetectionThreshold"].is<float>()) newSettings.jamDetectionThreshold = doc["jamDetectionThreshold"];
    
    if (doc["cellsInSeries"].is<int>()) newSettings.cellsInSeries = doc["cellsInSeries"];
    
    if (doc["ledBarNum"].is<int>()) newSettings.ledBarNum = doc["ledBarNum"];
    if (doc["ledBarBrightness"].is<int>()) newSettings.ledBarBrightness = doc["ledBarBrightness"];
    if (doc["ledBarBrightnessSecond"].is<int>()) newSettings.ledBarBrightnessSecond = doc["ledBarBrightnessSecond"];
    if (doc["ledFrequency"].is<int>()) newSettings.ledFrequency = doc["ledFrequency"];
    
    if (doc["lampMaxLevels"].is<int>()) {
        int oldVal = newSettings.lampMaxLevels;
        newSettings.lampMaxLevels = doc["lampMaxLevels"];
        String msg = "Updated lampMaxLevels: " + String(oldVal) + " -> " + String(newSettings.lampMaxLevels);
        log(msg.c_str());
        updatedFields++;
    }
    if (doc["lampBrightness"].is<JsonArray>()) {
        log("Processing lampBrightness array...");
        JsonArray lampArray = doc["lampBrightness"];
        String oldValues = "Old lampBrightness values: ";
        for (int i = 0; i < 10; i++) {
            oldValues += String(newSettings.lampBrightness[i]);
            if (i < 9) oldValues += ",";
        }
        log(oldValues.c_str());
        
        String arrayInfo = "JSON lampBrightness array size: " + String(lampArray.size());
        log(arrayInfo.c_str());
        
        for (int i = 0; i < 10 && i < lampArray.size(); i++) {
            int oldVal = newSettings.lampBrightness[i];
            newSettings.lampBrightness[i] = lampArray[i];
            String msg = "Updated lampBrightness[" + String(i) + "]: " + String(oldVal) + " -> " + String(newSettings.lampBrightness[i]);
            log(msg.c_str());
        }
        
        String newValues = "New lampBrightness values: ";
        for (int i = 0; i < 10; i++) {
            newValues += String(newSettings.lampBrightness[i]);
            if (i < 9) newValues += ",";
        }
        log(newValues.c_str());
        updatedFields++;
    }
    
    if (doc["wifiSSID"].is<const char*>()) {
        strncpy(newSettings.wifiSSID, doc["wifiSSID"], sizeof(newSettings.wifiSSID) - 1);
        newSettings.wifiSSID[sizeof(newSettings.wifiSSID) - 1] = '\0';
    }
    if (doc["wifiPassword"].is<const char*>()) {
        strncpy(newSettings.wifiPassword, doc["wifiPassword"], sizeof(newSettings.wifiPassword) - 1);
        newSettings.wifiPassword[sizeof(newSettings.wifiPassword) - 1] = '\0';
    }
    
    if (doc["beeperEnabled"].is<bool>()) {
        bool oldVal = newSettings.beeperEnabled;
        newSettings.beeperEnabled = doc["beeperEnabled"];
        String msg = "Updated beeperEnabled: " + String(oldVal ? "true" : "false") + " -> " + String(newSettings.beeperEnabled ? "true" : "false");
        log(msg.c_str());
        updatedFields++;
    }
    if (doc["debugLoggingEnabled"].is<bool>()) {
        bool oldVal = newSettings.debugLoggingEnabled;
        newSettings.debugLoggingEnabled = doc["debugLoggingEnabled"];
        String msg = "Updated debugLoggingEnabled: " + String(oldVal ? "true" : "false") + " -> " + String(newSettings.debugLoggingEnabled ? "true" : "false");
        log(msg.c_str());
        updatedFields++;
    }
    if (doc["standbyBlinkStartMinutes"].is<int>()) {
        int oldVal = newSettings.standbyBlinkStartMinutes;
        newSettings.standbyBlinkStartMinutes = doc["standbyBlinkStartMinutes"];
        String msg = "Updated standbyBlinkStartMinutes: " + String(oldVal) + " -> " + String(newSettings.standbyBlinkStartMinutes);
        log(msg.c_str());
        updatedFields++;
    }
    if (doc["standbyBlinkDurationSeconds"].is<int>()) {
        int oldVal = newSettings.standbyBlinkDurationSeconds;
        newSettings.standbyBlinkDurationSeconds = doc["standbyBlinkDurationSeconds"];
        String msg = "Updated standbyBlinkDurationSeconds: " + String(oldVal) + " -> " + String(newSettings.standbyBlinkDurationSeconds);
        log(msg.c_str());
        updatedFields++;
    }
    
    String summaryMsg = "Total fields updated from JSON: " + String(updatedFields);
    log(summaryMsg.c_str());
    
    // Validate new settings
    log("Validating new settings...");
    if (!validateSettings(newSettings)) {
        log("ERROR: New settings failed validation!");
        return false;
    }
    log("Settings validation passed");
    
    // Apply new settings
    log("Applying new settings to currentSettings...");
    currentSettings = newSettings;
    
    log("Calling saveSettings()...");
    saveSettings();
    
    // Log the new effective lamp settings
    String lampInfo = "NEW LAMP SETTINGS APPLIED - MaxLevels: " + String(getLampMaxLevels());
    for (int i = 0; i <= getLampMaxLevels(); i++) {
        lampInfo += ", L" + String(i) + ":" + String(getLampBrightness(i));
    }
    log(lampInfo.c_str());
    
    // Log the new effective motor settings
    String motorInfo = "NEW MOTOR SETTINGS APPLIED - SpeedSteps: " + String(getSpeedSteps()) +
                      ", StandbyDelay: " + String(getStandbyDelay()) + "s" +
                      ", BatteryMax: " + String(getBatteryPowerMax()) + "A" +
                      ", MinSpeed: " + String(getMinSpeedPercent(), 2) +
                      ", MaxRPM: " + String(getMaxSpeedRpm(), 0);
    log(motorInfo.c_str());
    
    String motorInfo2 = "MOTOR TIMING - SpeedUp: " + String(getSpeedUpTime()) + "ms" +
                       ", SpeedDown: " + String(getSpeedDownTime()) + "ms" +
                       ", MaxOverload: " + String(getMaxTimeOverloaded()) + "ms";
    log(motorInfo2.c_str());
    
    String jamInfo = "JAM DETECTION - Min: " + String(getJamMin(), 2) +
                    ", Threshold: " + String(getJamDetectionThreshold(), 2);
    log(jamInfo.c_str());
    
    String otherInfo = "OTHER SETTINGS - Beeper: " + String(getBeeperEnabled() ? "ON" : "OFF") +
                      ", Debug: " + String(getDebugLoggingEnabled() ? "ON" : "OFF") +
                      ", LEDBar: " + String(getLedBarNum()) + " LEDs";
    log(otherInfo.c_str());
    
    // Apply settings changes at runtime (no reboot required)
    log("Applying settings changes at runtime...");
    applyLampSettings(); // Update lamp PWM frequency and other lamp settings
    applyLedBarSettings(); // Update LED bar settings
    
    log("Settings updated and saved successfully - changes applied immediately");
    return true;
}

/**
 * Generate CSV data for a specific session file
 */
String generateSessionCsvData(String sessionFile) {
    log(("Starting CSV generation for session: " + sessionFile).c_str());
    
    // Ensure we have the full path
    String fullPath = sessionFile;
    if (!sessionFile.startsWith("/datalog/")) {
        fullPath = "/datalog/" + sessionFile;
    }
    
    String pathMsg = "CSV generation - File: " + sessionFile + ", Full path: " + fullPath;
    log(pathMsg.c_str());
    
    // Try to open the session file
    File file = LittleFS.open(fullPath, "r");
    if (!file) {
        String errorMsg = "Failed to open session file: " + sessionFile + " at " + fullPath;
        log(errorMsg.c_str());
        return "Error: Could not open session file " + sessionFile;
    }
    
    // Calculate how many data points we have
    size_t fileSize = file.size();
    size_t dataPointCount = fileSize / sizeof(LogdataRow);
    
    String fileSizeMsg = "Session file size: " + String(fileSize) + " bytes, estimated " + String(dataPointCount) + " data points";
    log(fileSizeMsg.c_str());
    
    // Build CSV header with Total Uptime as primary time reference
    String csv = "Total Uptime (s),Motor Temperature (degC),MOSFET Temperature (degC),Battery Voltage (V),Input Current (A),Motor Current (A),eRPM,Duty Cycle (%),Ambient Temperature (degC),Humidity (%),Battery Level (%),Leak Sensor State,LED Brightness (%),Left Button,Right Button,Beeper Enabled,Beeper Active\\r\\n";
    
    LogdataRow dataPoint;
    int exportedPoints = 0;
    int maxPoints = 1000; // Reasonable limit for ESP32 memory
    
    // Read and convert each data point
    for (size_t i = 0; i < dataPointCount && exportedPoints < maxPoints; i++) {
        size_t bytesRead = file.read((uint8_t*)&dataPoint, sizeof(LogdataRow));
        
        if (bytesRead != sizeof(LogdataRow)) {
            String errorMsg = "Error reading data point " + String(i) + " from session " + sessionFile + ", bytes read: " + String(bytesRead);
            log(errorMsg.c_str());
            break;
        }
        
        // Filter out corrupted data points with invalid boolean values
        if (dataPoint.batteryVoltage > 20.0 && dataPoint.batteryVoltage < 100.0 && // Valid battery voltage range
            (dataPoint.leftButton == 0 || dataPoint.leftButton == 1) &&             // Clean boolean values
            (dataPoint.rightButton == 0 || dataPoint.rightButton == 1) &&
            (dataPoint.beeperEnabled == 0 || dataPoint.beeperEnabled == 1) &&
            (dataPoint.beeperActive == 0 || dataPoint.beeperActive == 1)) {
            
            // Convert timestamp to total uptime in seconds
            float totalUptimeSeconds = dataPoint.totalUptime / 1000.0;
            
            // Build CSV row with Total Uptime first
            csv += String(totalUptimeSeconds, 1) + ",";
            csv += String(dataPoint.tempMotor, 1) + ",";
            csv += String(dataPoint.tempMosfet, 1) + ",";
            csv += String(dataPoint.batteryVoltage, 5) + ",";
            csv += String(dataPoint.current, 2) + ",";
            csv += String(dataPoint.avgMotorCurrent, 2) + ",";
            csv += String(dataPoint.erpm) + ",";
            csv += String(dataPoint.dutyCycle, 3) + ",";
            csv += String(dataPoint.temperature, 1) + ",";
            csv += String(dataPoint.humidity, 1) + ",";
                    csv += String(dataPoint.batteryLevel) + ",";
        csv += String(dataPoint.leakSensorState) + ",";
        csv += String(dataPoint.ledBrightness) + ",";
        csv += String(dataPoint.leftButton) + ",";
            csv += String(dataPoint.rightButton) + ",";
            csv += String(dataPoint.beeperEnabled) + ",";
            csv += String(dataPoint.beeperActive) + "\\r\\n";
            
            exportedPoints++;
        }
        
        // Check memory usage more frequently
        if (csv.length() > 400000) { // 400KB limit for ESP32 safety
            String limitMsg = "Session CSV memory limit reached at " + String(exportedPoints) + " points for " + sessionFile + ", exported " + String((float)exportedPoints/dataPointCount*100, 1) + "% of session";
            log(limitMsg.c_str());
            break;
        }
        
        // Give other tasks time to run
        if (i % 10 == 0) {
            vTaskDelay(1 / portTICK_PERIOD_MS);
        }
    }
    
    file.close();
    
    String resultMsg = "Generated session CSV for " + sessionFile + " with " + String(exportedPoints) + "/" + String(dataPointCount) + " points, size: " + String(csv.length()) + " bytes";
    log(resultMsg.c_str());
    
    // Add summary footer if truncated
    if (exportedPoints < dataPointCount) {
        csv += "\\r\\n# Note: Session truncated due to memory limits\\r\\n";
        csv += "# Exported " + String(exportedPoints) + " of " + String(dataPointCount) + " total points (" + String((float)exportedPoints/dataPointCount*100, 1) + "%)\\r\\n";
        csv += "# Use session data API for complete dataset\\r\\n";
    }
    
    return csv;
}

/**
 * Stream CSV data directly to client to avoid memory issues
 */
void streamSessionCsvData(WiFiClient client, String sessionFile) {
    log(("Starting streaming CSV for session: " + sessionFile).c_str());
    
    // Ensure we have the full path
    String fullPath = sessionFile;
    if (!sessionFile.startsWith("/datalog/")) {
        fullPath = "/datalog/" + sessionFile;
    }
    
    String pathMsg = "CSV streaming - File: " + sessionFile + ", Full path: " + fullPath;
    log(pathMsg.c_str());
    
    // Try to open the session file
    File file = LittleFS.open(fullPath, "r");
    if (!file) {
        String errorMsg = "Failed to open session file: " + sessionFile + " at " + fullPath;
        log(errorMsg.c_str());
        String errorResponse = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nError: Could not open session file " + sessionFile;
        client.print(errorResponse);
        return;
    }
    
    // Calculate how many data points we have
    size_t fileSize = file.size();
    size_t dataPointCount = fileSize / sizeof(LogdataRow);
    
    String fileSizeMsg = "Session file size: " + String(fileSize) + " bytes, estimated " + String(dataPointCount) + " data points";
    log(fileSizeMsg.c_str());
    
    // Send HTTP headers for CSV download
    client.print("HTTP/1.1 200 OK\r\n");
    client.print("Content-Type: text/csv\r\n");
    client.print("Content-Disposition: attachment; filename=\"" + sessionFile.substring(0, sessionFile.lastIndexOf('.')) + ".csv\"\r\n");
    client.print("Cache-Control: no-cache\r\n");
    client.print("\r\n");
    
    // Send CSV header
    client.print("Total Uptime (s),Motor Temperature (degC),MOSFET Temperature (degC),Battery Voltage (V),Input Current (A),Motor Current (A),eRPM,Duty Cycle (%),Ambient Temperature (degC),Humidity (%),Battery Level (%),Leak Sensor State,LED Brightness (%),Left Button,Right Button,Beeper Enabled,Beeper Active\r\n");
    
    LogdataRow dataPoint;
    int exportedPoints = 0;
    int maxPoints = 10000; // Much higher limit since we're streaming
    
    // Stream each data point directly to client
    for (size_t i = 0; i < dataPointCount && exportedPoints < maxPoints; i++) {
        size_t bytesRead = file.read((uint8_t*)&dataPoint, sizeof(LogdataRow));
        
        if (bytesRead != sizeof(LogdataRow)) {
            String errorMsg = "Error reading data point " + String(i) + " from session " + sessionFile;
            log(errorMsg.c_str());
            break;
        }
        
        // Filter out corrupted data points with invalid boolean values
        if (dataPoint.batteryVoltage > 20.0 && dataPoint.batteryVoltage < 100.0 && // Valid battery voltage range
            (dataPoint.leftButton == 0 || dataPoint.leftButton == 1) &&             // Clean boolean values
            (dataPoint.rightButton == 0 || dataPoint.rightButton == 1) &&
            (dataPoint.beeperEnabled == 0 || dataPoint.beeperEnabled == 1) &&
            (dataPoint.beeperActive == 0 || dataPoint.beeperActive == 1)) {
            
            // Convert timestamp to total uptime in seconds
            float totalUptimeSeconds = dataPoint.totalUptime / 1000.0;
            
            // Build and send CSV row directly (small string, immediately sent)
            String csvRow = String(totalUptimeSeconds, 1) + ",";
            csvRow += String(dataPoint.tempMotor, 1) + ",";
            csvRow += String(dataPoint.tempMosfet, 1) + ",";
            csvRow += String(dataPoint.batteryVoltage, 5) + ",";
            csvRow += String(dataPoint.current, 2) + ",";
            csvRow += String(dataPoint.avgMotorCurrent, 2) + ",";
            csvRow += String(dataPoint.erpm) + ",";
            csvRow += String(dataPoint.dutyCycle, 3) + ",";
            csvRow += String(dataPoint.temperature, 1) + ",";
            csvRow += String(dataPoint.humidity, 1) + ",";
            csvRow += String(dataPoint.batteryLevel) + ",";
            csvRow += String(dataPoint.leakSensorState) + ",";
            csvRow += String(dataPoint.ledBrightness) + ",";
            csvRow += String(dataPoint.leftButton) + ",";
            csvRow += String(dataPoint.rightButton) + ",";
            csvRow += String(dataPoint.beeperEnabled) + ",";
            csvRow += String(dataPoint.beeperActive) + "\r\n";
            
            // Send this row immediately
            client.print(csvRow);
            
            exportedPoints++;
        }
        
        // Give other tasks time and check client connection
        if (i % 10 == 0) {
            vTaskDelay(1 / portTICK_PERIOD_MS);
            if (!client.connected()) {
                log("Client disconnected during CSV stream");
                break;
            }
        }
        
        // Progress logging
        if (exportedPoints % 100 == 0) {
            String progressMsg = "Streamed " + String(exportedPoints) + "/" + String(dataPointCount) + " CSV rows";
            log(progressMsg.c_str());
        }
    }
    
    file.close();
    
    // Send summary footer if truncated
    if (exportedPoints < dataPointCount) {
        client.print("\r\n# Note: Session truncated due to data limits\r\n");
        client.print("# Exported " + String(exportedPoints) + " of " + String(dataPointCount) + " total points (" + String((float)exportedPoints/dataPointCount*100, 1) + "%)\r\n");
    }
    
    String resultMsg = "Completed streaming CSV for " + sessionFile + " with " + String(exportedPoints) + "/" + String(dataPointCount) + " points";
    log(resultMsg.c_str());
}


// Setup the webserver task on Core 0
void setupWebserver() {
    log("Setting up webserver on Core 0");
    
    // Initialize SPIFFS and store HTML files
    spiffsInitialized = initializeFileSystem();
    
    // Create task on Core 0
    xTaskCreatePinnedToCore(
        webserverTask,         // Task function
        "WebserverTask",       // Task name
        10000,                 // Stack size (bytes)
        NULL,                  // Task parameters
        1,                     // Task priority (1 is low)
        &webserverTaskHandle,  // Task handle
        0                      // Core ID (0)
    );
    
    log("Webserver task created on Core 0");
}

// Process HTTP requests
void handleClient(WiFiClient client) {
    // Wait for data to be available
    unsigned long timeout = millis() + 5000; // 5 second timeout
    while (!client.available() && millis() < timeout) {
        delay(10);
    }
    
    // If no data, close connection and return
    if (!client.available()) {
        client.stop();
        return;
    }
    
    // Read the complete HTTP request
    String httpRequest = "";
    String line = "";
    String method = "";
    String path = "";
    String host = "";
    String contentLength = "";
    
    // Read request line
    line = client.readStringUntil('\n');
    httpRequest += line;
    
    // Extract method and path from first line
    int firstSpace = line.indexOf(' ');
    int secondSpace = line.indexOf(' ', firstSpace + 1);
    
    if (firstSpace != -1 && secondSpace != -1) {
        method = line.substring(0, firstSpace);
        path = line.substring(firstSpace + 1, secondSpace);
    }
    
    log(("Request: " + method + " " + path).c_str());
    
    // Read headers
    while (client.connected()) {
        line = client.readStringUntil('\n');
        line.trim();
        httpRequest += line + "\n";
        
        if (line.startsWith("Host: ")) {
            host = line.substring(6);
            log(("Host: " + host).c_str());
        }
        
        if (line.startsWith("Content-Length: ")) {
            contentLength = line.substring(16);
        }
        
        // Empty line indicates end of headers
        if (line.length() == 0) {
            break;
        }
    }
    
    // Check if this is a captive portal detection request
    bool isCaptivePortalRequest = host.length() > 0 && 
                                 !host.equals(apIP.toString()) &&
                                 !host.startsWith("4.3.2.") &&
                                 !host.equals("localhost") &&
                                 !host.equals("captive.apple.com");
    
    // Handle the request based on the path
    if (path == "/" || path == "/index.html") {
        // Root path - serve HTML from embedded files or LittleFS fallback
        if (loadFromEmbeddedOrSPIFFS(client, "/index.html")) {
            log("Served index.html from embedded files or LittleFS");
        } else {
            sendHttpResponse(client, 404, "text/plain", "index.html not found");
        }
    } else if (path == "/api/data" || path.startsWith("/api/data?")) {
        // API endpoint for datalogger data
        String range = "recent";
        int count = 100;
        
        // Parse query parameters
        if (path.indexOf("?") != -1) {
            String queryString = path.substring(path.indexOf("?") + 1);
            
            // Extract count parameter
            int countIndex = queryString.indexOf("count=");
            if (countIndex != -1) {
                String countStr = queryString.substring(countIndex + 6);
                int ampIndex = countStr.indexOf("&");
                if (ampIndex != -1) {
                    countStr = countStr.substring(0, ampIndex);
                }
                count = countStr.toInt();
                if (count <= 0 || count > 1000) count = 100; // Limit to reasonable range
            }
            
            // Extract range parameter
            int rangeIndex = queryString.indexOf("range=");
            if (rangeIndex != -1) {
                String rangeStr = queryString.substring(rangeIndex + 6);
                int ampIndex = rangeStr.indexOf("&");
                if (ampIndex != -1) {
                    rangeStr = rangeStr.substring(0, ampIndex);
                }
                range = rangeStr;
            }
        }
        
        String jsonData = generateDataLoggerJson(count, range);
        sendHttpResponse(client, 200, "application/json", jsonData.c_str());
        
    } else if (path == "/api/status") {
        // API endpoint for system status
        log("API /api/status called");
        
        String json = "{";
        json += "\"status\":\"ok\",";
        json += "\"uptime\":" + String(millis()) + ",";
        json += "\"totalUptime\":" + String(getTotalUptime()) + ",";
        json += "\"dataPoints\":" + String(getTotalDataPoints("recent")) + ",";
        json += "\"motor\":" + String(motorState == on ? "true" : "false") + ",";
        json += "\"lamp\":" + String(LED_State > 0 ? "true" : "false") + ",";
        json += "\"lampLevel\":" + String(LED_State) + ",";
        json += "\"beeper\":" + String(getBeeperEnabled() ? "true" : "false") + ",";
        json += "\"beeperEnabled\":" + String(getBeeperEnabled() ? "true" : "false") + ",";
        json += "\"beeperActive\":" + String(isBeeperActive() ? "true" : "false") + ",";
        json += "\"erpm\":" + String(getVescUart().data.rpm) + ",";
        json += "\"leftButton\":" + String(leftButtonState == PRESSED ? "true" : "false") + ",";
        json += "\"rightButton\":" + String(rightButtonState == PRESSED ? "true" : "false") + ",";
        json += "\"waterSensorFront\":" + String(digitalRead(PIN_LEAK_FRONT) == LOW ? "true" : "false") + ",";
        json += "\"waterSensorBack\":" + String(digitalRead(PIN_LEAK_BACK) == LOW ? "true" : "false") + ",";
        
        // Add sensor data for status display
        if (HAS_MOTOR) {
            json += "\"batteryVoltage\":" + String(getVescUart().data.inpVoltage) + ",";
            json += "\"motorTemperature\":" + String(getVescUart().data.tempMotor) + ",";
            json += "\"mosfetTemperature\":" + String(getVescUart().data.tempMosfet) + ",";
            json += "\"current\":" + String(getVescUart().data.avgInputCurrent) + ",";
            json += "\"motorCurrent\":" + String(getVescUart().data.avgMotorCurrent) + ",";
            json += "\"dutyCycle\":" + String(getVescUart().data.dutyCycleNow) + ",";
            json += "\"rpm\":" + String(getVescUart().data.rpm);
        } else {
            // Fallback values if no motor
            json += "\"batteryVoltage\":48.0,";
            json += "\"motorTemperature\":25.0,";
            json += "\"mosfetTemperature\":30.0,";
            json += "\"current\":0.0,";
            json += "\"motorCurrent\":0.0,";
            json += "\"dutyCycle\":0.0,";
            json += "\"rpm\":0";
        }
        
        // Add environmental sensor data
        TempAndHumidity envData = dhtSensor.getTempAndHumidity();
        if (!isnan(envData.temperature) && !isnan(envData.humidity)) {
            json += ",\"temperature\":" + String(envData.temperature);
            json += ",\"humidity\":" + String(envData.humidity);
        } else {
            json += ",\"temperature\":22.0";
            json += ",\"humidity\":50.0";
        }
        
        // Add battery level
        json += ",\"batteryLevel\":" + String(batteryLevel);
        
        json += "}";
        
        sendHttpResponse(client, 200, "application/json", json.c_str());
        
    } else if (path == "/api/sessions") {
        // API endpoint for session list
        log("API /api/sessions called");
        
        String sessionsJson = generateSessionListJson();
        String sessionsMsg = "Sessions JSON generated - Length: " + String(sessionsJson.length());
        log(sessionsMsg.c_str());
        
        if (sessionsJson.length() > 100) {
            String preview = "Sessions preview: " + sessionsJson.substring(0, 100) + "...";
            log(preview.c_str());
        } else {
            String full = "Sessions full: " + sessionsJson;
            log(full.c_str());
        }
        
        sendHttpResponse(client, 200, "application/json", sessionsJson.c_str());
        
    } else if (path.startsWith("/api/sessions/") && path.endsWith("/data") && method == "GET") {
        // API endpoint for session data
        String sessionFile = path.substring(14); // Remove "/api/sessions/"
        sessionFile = sessionFile.substring(0, sessionFile.length() - 5); // Remove "/data"
        
        log(("API session data request for: " + sessionFile).c_str());
        
        String sessionData = generateSessionDataJson(sessionFile);
        sendHttpResponse(client, 200, "application/json", sessionData.c_str());
        
    } else if (path.startsWith("/api/sessions/") && path.endsWith("/csv") && method == "GET") {
        // API endpoint for session CSV download
        String sessionFile = path.substring(14); // Remove "/api/sessions/"
        sessionFile = sessionFile.substring(0, sessionFile.length() - 4); // Remove "/csv"
        
        log(("API session CSV request for: " + sessionFile).c_str());
        
        // Stream CSV data directly to avoid memory issues
        streamSessionCsvData(client, sessionFile);
        return; // streamSessionCsvData handles client connection
        
    } else if (path == "/api/settings" && method == "GET") {
        // API endpoint to get current settings
        log("API /api/settings GET called");
        
        String settingsJson = generateSettingsJson();
        sendHttpResponse(client, 200, "application/json", settingsJson.c_str());
        
    } else if (path == "/api/settings" && method == "POST") {
        // API endpoint to save settings
        log("API /api/settings POST called");
        
        // Read POST body if Content-Length is specified
        String body = "";
        if (contentLength.length() > 0) {
            int bodyLength = contentLength.toInt();
            if (bodyLength > 0 && bodyLength < 10240) { // 10KB limit for settings
                char* buffer = new char[bodyLength + 1];
                int bytesRead = 0;
                unsigned long startTime = millis();
                
                // Read the exact number of bytes specified in Content-Length
                while (bytesRead < bodyLength && client.connected() && (millis() - startTime < 3000)) {
                    if (client.available()) {
                        buffer[bytesRead] = client.read();
                        bytesRead++;
                    } else {
                        delay(1);
                    }
                }
                
                buffer[bytesRead] = '\0';
                body = String(buffer);
                delete[] buffer;
                
                String readMsg = "Settings: Read " + String(bytesRead) + " bytes of " + String(bodyLength) + " expected";
                log(readMsg.c_str());
            } else {
                log("Settings: Invalid Content-Length or too large");
            }
        } else {
            // Fallback: read whatever is available (old method)
            delay(100); // Give more time for settings data to arrive
            while (client.available()) {
                body += (char)client.read();
            }
            log("Settings: Using fallback reading method");
        }
        
        String bodyMsg = "Settings POST body received, length: " + String(body.length());
        log(bodyMsg.c_str());
        
        if (body.length() > 100) {
            String bodyPreview = "Settings body preview: " + body.substring(0, 100) + "...";
            log(bodyPreview.c_str());
        } else if (body.length() > 0) {
            String bodyFull = "Settings body full: " + body;
            log(bodyFull.c_str());
        } else {
            log("Settings: ERROR - No body data received!");
        }
        
        bool success = false;
        String errorMsg = "";
        if (body.length() > 0) {
            success = updateSettingsFromJson(body);
            if (!success) {
                errorMsg = "Settings validation or parsing failed. See device log for details.";
            }
        } else {
            log("Settings: Cannot save - empty body");
            errorMsg = "No settings data received.";
        }
        
        String response;
        if (success) {
            response = "{\"success\":true}";
        } else {
            response = "{\"success\":false,\"error\":\"" + errorMsg + "\"}";
        }
        sendHttpResponse(client, 200, "application/json", response.c_str());
        
    } else if (path == "/api/settings/restore" && method == "POST") {
        // API endpoint to restore default settings
        log("API /api/settings/restore called");
        
        restoreDefaultSettings();
        
        // Apply settings changes at runtime (no reboot required)
        log("Applying default settings at runtime...");
        applyLampSettings(); // Update lamp PWM frequency and other lamp settings
        applyLedBarSettings(); // Update LED bar settings
        
        String response = "{\"success\":true}";
        sendHttpResponse(client, 200, "application/json", response.c_str());
        
    } else if (path == "/api/reboot" && method == "POST") {
        // API endpoint to reboot the system
        log("API /api/reboot called");
        
        String response = "{\"success\":true,\"message\":\"Reboot initiated\"}";
        sendHttpResponse(client, 200, "application/json", response.c_str());
        
        // Close client connection properly before rebooting
        client.stop();
        
        // Wait a moment to ensure response is sent
        delay(500);
        
        // Reboot the ESP32
        log("System reboot requested via API - restarting now");
        ESP.restart();
        
    } else if (path == "/api/motor" && method == "POST") {
        // API endpoint for motor control
        log("API /api/motor called");
        
        // Read POST body if Content-Length is specified
        String body = "";
        if (contentLength.length() > 0) {
            int bodyLength = contentLength.toInt();
            if (bodyLength > 0 && bodyLength < 2048) { // Reasonable limit
                char* buffer = new char[bodyLength + 1];
                int bytesRead = 0;
                unsigned long startTime = millis();
                
                // Read the exact number of bytes specified in Content-Length
                while (bytesRead < bodyLength && client.connected() && (millis() - startTime < 2000)) {
                    if (client.available()) {
                        buffer[bytesRead] = client.read();
                        bytesRead++;
                    } else {
                        delay(1);
                    }
                }
                
                buffer[bytesRead] = '\0';
                body = String(buffer);
                delete[] buffer;
                
                String readMsg = "Read " + String(bytesRead) + " bytes of " + String(bodyLength) + " expected";
                log(readMsg.c_str());
            }
        } else {
            // Fallback: read whatever is available
            delay(50); // Give time for data to arrive
            while (client.available()) {
                body += (char)client.read();
            }
        }
        
        String bodyMsg = "Motor control body: " + body;
        log(bodyMsg.c_str());
        
        // Simple JSON parsing for motor control
        bool motorEnabled = body.indexOf("\"enabled\":true") != -1;
        int speed = 0;
        
        // Extract speed value
        int speedIndex = body.indexOf("\"speed\":");
        if (speedIndex != -1) {
            String speedStr = body.substring(speedIndex + 8);
            int endIndex = speedStr.indexOf(',');
            if (endIndex == -1) endIndex = speedStr.indexOf('}');
            if (endIndex != -1) {
                speedStr = speedStr.substring(0, endIndex);
                speed = speedStr.toInt();
            }
        }
        
        // Integrate with actual motor control functions
        String controlMsg = "Remote motor control - Enabled: " + String(motorEnabled ? "true" : "false") + ", Speed: " + String(speed) + "%";
        log(controlMsg.c_str());
        
        if (motorEnabled && speed > 0) {
            // Enable remote control mode
            remoteControlActive = true;
            
            // Wake up motor if in standby
            if (motorState == standby) {
                wakeUp();
            }
            
            // Convert speed percentage (0-100) to motor steps (1-maxSteps)
            int maxSteps = getSpeedSteps();
            int targetStep = max(1, min(maxSteps, (speed * maxSteps) / 100));
            currentMotorStep = targetStep;
            motorState = on;
            
            // Update lastActionTime to keep motor running (simulates button press)
            lastActionTime = micros();
            
            // Update LED bar to show new speed
            setBarSpeed(currentMotorStep);
            
            String speedMsg = "Remote control set motor to step " + String(currentMotorStep) + " (speed " + String(speed) + "%)";
            log(speedMsg.c_str());
            
        } else {
            // Disable remote control mode and stop motor
            remoteControlActive = false;
            motorState = off;
            lastActionTime = micros(); // Prevent immediate standby
            setBarSpeed(currentMotorStep); // Update display but keep step setting
            
            log("Remote control stopped motor");
        }
        
        String response = "{\"success\":true,\"enabled\":" + String(motorEnabled ? "true" : "false") + ",\"speed\":" + String(speed) + ",\"motorStep\":" + String(currentMotorStep) + "}";
        sendHttpResponse(client, 200, "application/json", response.c_str());
        
    } else if (path == "/api/lamp" && method == "POST") {
        // API endpoint for lamp control
        log("API /api/lamp called");
        
        // Read POST body if Content-Length is specified
        String body = "";
        if (contentLength.length() > 0) {
            int bodyLength = contentLength.toInt();
            if (bodyLength > 0 && bodyLength < 2048) { // Reasonable limit
                char* buffer = new char[bodyLength + 1];
                int bytesRead = 0;
                unsigned long startTime = millis();
                
                // Read the exact number of bytes specified in Content-Length
                while (bytesRead < bodyLength && client.connected() && (millis() - startTime < 2000)) {
                    if (client.available()) {
                        buffer[bytesRead] = client.read();
                        bytesRead++;
                    } else {
                        delay(1);
                    }
                }
                
                buffer[bytesRead] = '\0';
                body = String(buffer);
                delete[] buffer;
                
                String readMsg = "Read " + String(bytesRead) + " bytes of " + String(bodyLength) + " expected";
                log(readMsg.c_str());
            }
        } else {
            // Fallback: read whatever is available
            delay(50); // Give time for data to arrive
            while (client.available()) {
                body += (char)client.read();
            }
        }
        
        String bodyMsg = "Lamp control body: " + body;
        log(bodyMsg.c_str());
        
        // Extract level value (now direct level 0-maxLevels)
        int requestedLevel = 0;
        int levelIndex = body.indexOf("\"level\":");
        if (levelIndex != -1) {
            String levelStr = body.substring(levelIndex + 8);
            int endIndex = levelStr.indexOf(',');
            if (endIndex == -1) endIndex = levelStr.indexOf('}');
            if (endIndex != -1) {
                levelStr = levelStr.substring(0, endIndex);
                requestedLevel = levelStr.toInt();
            }
        }
        
        // Validate level against current settings
        int maxLevels = getLampMaxLevels();
        int actualLevel = requestedLevel;
        
        // Validate level range
        if (actualLevel < 0) actualLevel = 0;
        if (actualLevel >= maxLevels) actualLevel = maxLevels - 1;
        
        // Integrate with actual LED lamp functions
        String controlMsg = "Remote lamp control - Requested Level: " + String(requestedLevel) + ", Actual Level: " + String(actualLevel);
        log(controlMsg.c_str());
        
        // Set level
        LED_State = actualLevel;
        setLEDState(LED_State);
        setBarLED(LED_State);
        
        String levelMsg = "Remote control set lamp to level " + String(actualLevel) + " (max: " + String(maxLevels - 1) + ")";
        log(levelMsg.c_str());
        
        String response = "{\"success\":true,\"level\":" + String(actualLevel) + ",\"maxLevels\":" + String(maxLevels) + "}";
        sendHttpResponse(client, 200, "application/json", response.c_str());
        
    } else if (path == "/api/version") {
        // API endpoint for version information
        log("API /api/version called");
        
        String version = "2.0.0"; // Default version
        
        // Try to read version from embedded file first, then LittleFS fallback
        const EmbeddedFile* versionFile = nullptr;
        #ifdef HAS_EMBEDDED_FILES
        versionFile = findEmbeddedFile("version.txt");
        #endif
        
        if (versionFile != nullptr) {
            // Read from embedded file
            version = String((const char*)versionFile->data);
            version.trim(); // Remove whitespace
            log("Version read from embedded file");
        } else if (LittleFS.exists("/version.txt")) {
            // Fallback to LittleFS
            File versionFileFS = LittleFS.open("/version.txt", "r");
            if (versionFileFS) {
                version = versionFileFS.readString();
                version.trim(); // Remove whitespace
                versionFileFS.close();
                log("Version read from LittleFS");
            }
        }
        
        String jsonVersion = "{\"version\":\"" + version + "\"}";
        sendHttpResponse(client, 200, "application/json", jsonVersion.c_str());
        
    } else if (path == "/api/delete-all-sessions" && method == "POST") {
        // API endpoint to delete all session files
        log("API /api/delete-all-sessions called");
        
        int deleteCount = 0;
        String errorMsg = "";
        bool success = true;
        
        try {
            // Directly iterate through datalog directory to find all .bin files
            // This avoids the 50-session limit from listSessionFiles()
            File root = LittleFS.open("/datalog");
            if (root && root.isDirectory()) {
                File file = root.openNextFile();
                while (file) {
                    String fileName = String(file.name());
                    if (!file.isDirectory() && fileName.endsWith(".bin")) {
                        String fullPath = "/datalog/" + fileName;
                        file.close(); // Close file handle before deletion
                        
                        if (LittleFS.exists(fullPath)) {
                            if (LittleFS.remove(fullPath)) {
                                deleteCount++;
                                String delMsg = "Deleted session file: " + fullPath;
                                log(delMsg.c_str());
                            } else {
                                errorMsg += "Failed to delete " + fileName + "; ";
                                success = false;
                            }
                        } else {
                            errorMsg += "File not found " + fileName + "; ";
                        }
                    } else {
                        file.close(); // Close non-.bin files
                    }
                    file = root.openNextFile();
                }
                root.close();
            } else {
                errorMsg = "Could not open /datalog directory";
                success = false;
            }
            
            String resultMsg = "Deleted " + String(deleteCount) + " session files";
            log(resultMsg.c_str());
            
        } catch (...) {
            errorMsg = "Exception occurred during deletion";
            success = false;
        }
        
        String response;
        if (success && deleteCount > 0) {
            response = "{\"success\":true,\"deleted\":" + String(deleteCount) + ",\"message\":\"Successfully deleted " + String(deleteCount) + " session files\"}";
        } else if (deleteCount == 0) {
            response = "{\"success\":true,\"deleted\":0,\"message\":\"No session files found to delete\"}";
        } else {
            response = "{\"success\":false,\"deleted\":" + String(deleteCount) + ",\"error\":\"" + errorMsg + "\"}";
        }
        
        sendHttpResponse(client, 200, "application/json", response.c_str());
        
    } else if (path == "/info.html") {
        // Serve info page
        if (loadFromEmbeddedOrSPIFFS(client, "/info.html")) {
            log("Served info.html from embedded files or LittleFS");
        } else {
            sendHttpResponse(client, 404, "text/plain", "Info page not found");
        }
        
    } else if (path == "/remote.html") {
        // Serve remote control page
        if (loadFromEmbeddedOrSPIFFS(client, "/remote.html")) {
            log("Served remote.html from embedded files or LittleFS");
        } else {
            sendHttpResponse(client, 404, "text/plain", "Remote control page not found");
        }
        
    } else if (path == "/settings.html") {
        // Serve settings page
        if (loadFromEmbeddedOrSPIFFS(client, "/settings.html")) {
            log("Served settings.html from embedded files or LittleFS");
        } else {
            sendHttpResponse(client, 404, "text/plain", "Settings page not found");
        }
        

        
    } else if (path == "/chart.min.js") {
        // Try to serve Chart.js from LittleFS first
        if (LittleFS.exists("/chart.min.js")) {
            File chartFile = LittleFS.open("/chart.min.js", "r");
            if (chartFile) {
                log("Serving Chart.js 4.4.9 from LittleFS");
                size_t fileSize = chartFile.size();
                
                // Send HTTP headers first
                client.print("HTTP/1.1 200 OK\r\n");
                client.print("Content-Type: application/javascript\r\n");
                client.print("Content-Length: ");
                client.print(fileSize);
                client.print("\r\n");
                client.print("Cache-Control: public, max-age=86400\r\n");
                client.print("\r\n");
                
                // Stream file in chunks to avoid watchdog timeout
                const size_t CHUNK_SIZE = 1024;
                uint8_t buffer[CHUNK_SIZE];
                size_t totalSent = 0;
                
                while (chartFile.available() && totalSent < fileSize) {
                    size_t bytesToRead = min(CHUNK_SIZE, fileSize - totalSent);
                    size_t bytesRead = chartFile.read(buffer, bytesToRead);
                    
                    if (bytesRead > 0) {
                        client.write(buffer, bytesRead);
                        totalSent += bytesRead;
                        
                        // Feed watchdog every chunk
                        yield();
                        
                        // Small delay to prevent overwhelming the client
                        if (totalSent % (CHUNK_SIZE * 4) == 0) {
                            delay(1);
                        }
                    } else {
                        break;
                    }
                }
                
                chartFile.close();
                log(("Chart.js served successfully, " + String(totalSent) + " bytes").c_str());
            } else {
                log("Error: Could not open Chart.js file");
                sendHttpResponse(client, 404, "text/plain", "Chart.js not found");
            }
        } else {
            log("Chart.js file not found in LittleFS");
            sendHttpResponse(client, 404, "text/plain", "Chart.js not found");
        }
        
    } else if (path == "/jszip.min.js") {
        // Try to serve JSZip from LittleFS first
        if (LittleFS.exists("/jszip.min.js")) {
            File jszipFile = LittleFS.open("/jszip.min.js", "r");
            if (jszipFile) {
                log("Serving JSZip from LittleFS");
                size_t fileSize = jszipFile.size();
                
                // Send HTTP headers first
                client.print("HTTP/1.1 200 OK\r\n");
                client.print("Content-Type: application/javascript\r\n");
                client.print("Content-Length: ");
                client.print(fileSize);
                client.print("\r\n");
                client.print("Cache-Control: public, max-age=86400\r\n");
                client.print("\r\n");
                
                // Stream file in chunks
                const size_t CHUNK_SIZE = 1024;
                uint8_t buffer[CHUNK_SIZE];
                size_t totalSent = 0;
                
                while (jszipFile.available() && totalSent < fileSize) {
                    size_t bytesToRead = min(CHUNK_SIZE, fileSize - totalSent);
                    size_t bytesRead = jszipFile.read(buffer, bytesToRead);
                    
                    if (bytesRead > 0) {
                        client.write(buffer, bytesRead);
                        totalSent += bytesRead;
                        yield(); // Feed watchdog
                    } else {
                        break;
                    }
                }
                
                jszipFile.close();
                log(("JSZip served successfully, " + String(totalSent) + " bytes").c_str());
            } else {
                log("Error: Could not open JSZip file");
                sendHttpResponse(client, 404, "text/plain", "JSZip not found");
            }
        } else {
            log("JSZip file not found in LittleFS");
            sendHttpResponse(client, 404, "text/plain", "JSZip not found");
        }
        
    } else if (path == "/api/beeper" && method == "POST") {
        // API endpoint for beeper settings (legacy compatibility)
        log("API /api/beeper called");
        
        // Read POST body
        String body = "";
        while (client.available()) {
            body += (char)client.read();
        }
        
        // Simple JSON parsing for {"enabled": true/false}
        bool newBeeperState = body.indexOf("\"enabled\":true") != -1;
        
        // Update beeper setting in both old and new systems
        currentSettings.beeperEnabled = newBeeperState;
        saveSettings(); // Save to unified settings system
        
        String response = "{\"success\":true,\"enabled\":" + 
                         String(getBeeperEnabled() ? "true" : "false") + "}";
        sendHttpResponse(client, 200, "application/json", response.c_str());
        
        String beeperMsg = "Beeper setting updated: " + 
                          String(getBeeperEnabled() ? "enabled" : "disabled");
        log(beeperMsg.c_str());
        
    } else if (path == "/generate_204" || path == "/ncsi.txt" || 
               path == "/connecttest.txt" || path == "/redirect" || 
               path == "/hotspot-detect.html" || 
               path.indexOf("success.txt") != -1 || 
               path.indexOf("success.html") != -1) {
        
        // Android/Windows/iOS captive portal detection
        log("Captive portal check detected");
        sendHttpResponse(client, 302, "text/html", 
            "<html><head><meta http-equiv='refresh' content='0; "
            "URL=http://4.3.2.1/'></head><body>Redirecting...</body></html>");
    
    } else if (serveEmbeddedFile(client, path.c_str())) {
        // Served from embedded files
        log("Served file from embedded storage");
    } else if (spiffsInitialized && LittleFS.exists(path)) {
        // Serve files from SPIFFS
        loadFromSPIFFS(client, path);
    } else if (isCaptivePortalRequest) {
        // Captive portal detection - redirect to our server
        log("Captive portal request detected");
        sendHttpResponse(client, 302, "text/html", 
            "<html><head><meta http-equiv='refresh' content='0; "
            "URL=http://4.3.2.1/'></head><body>Redirecting...</body></html>");
    } else {
        // Default: redirect to root
        sendHttpResponse(client, 302, "text/plain", "Redirecting...");
    }
    
    // Close the connection
    client.stop();
}

// Webserver task that runs on Core 0
void webserverTask(void *pvParameters) {
    log("Webserver task started on Core 0");
    
    // Setup WiFi Access Point
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(getWifiSSID(), getWifiPassword());
    
    // Log IP address - convert to String and then to char*
    String ipString = "IP: " + WiFi.softAPIP().toString();
    log(ipString.c_str());
    
    // Start DNS Server for captive portal - redirect all requests to our IP
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(DNS_PORT, "*", apIP);
    log("DNS Server started - redirecting all domains to captive portal");
    
    // Start server
    server.begin();
    log("HTTP server started");
    
    // Main loop for webserver task
    while (true) {
        // Process DNS requests for captive portal
        dnsServer.processNextRequest();
        
        // Check for HTTP clients
        WiFiClient client = server.available();
        if (client) {
            handleClient(client);
        }
        
        // Small delay to prevent watchdog trigger
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
} 
