#define ARDUINOJSON_ENABLE_PROGMEM 0
#define ARDUINOJSON_ENABLE_FLASH_STRING 0
#include "mock_webserver.h"
#include "mock_hardware.h"
#include "mock_datalog.h"
#include "ArduinoJson_config.h"
#include "ArduinoJson.h"
#include <string>

String mockResponse = "";
extern bool mockRebootCalled;

// Mock session data for webserver tests
LogdataRow* mockSessionData = nullptr;
int mockSessionDataCount = 0;

void handleClient() {
    // Dummy implementation
}

void sendHttpResponse(const char* response) {
    mockResponse = response ? response : "";
}

// Dummy API handler implementations for native tests
extern "C" {
    void handleApiStatus() {
        mockResponse = "{\"status\":\"online\",\"uptime\":10," + 
                       String("\"totalUptime\":") + String("1010") + 
                       ",\"motor\":false,\"lamp\":false,\"beeper\":true,\"erpm\":0," +
                       "\"leftButton\":false,\"rightButton\":false}";
    }

    void handleApiMotor() {
        mockResponse = "{\"success\":true}";
    }

    void handleApiLamp() {
        mockResponse = "{\"success\":true}";
    }

    void handleApiBeeper() {
        mockResponse = "{\"success\":true}";
    }

    void handleApiSettings() {
        mockResponse = "{\"motorEnabled\":true,\"lampEnabled\":true,\"beeperEnabled\":true," +
                       String("\"motorDuration\":30,\"lampDuration\":15,\"beeperDuration\":5}");
    }

    void handleApiVersion() {
        mockResponse = "{\"version\":\"test-1.0.0\"}";
    }
}

// Mock session data generation for testing
String generateSessionDataJson(String sessionFile) {
    if (!mockSessionData || mockSessionDataCount == 0) {
        return "{\"data\":[],\"meta\":{\"error\":\"No mock data\"}}";
    }
    
    JsonDocument doc;
    
    // Calculate metadata from mock data
    LogdataRow firstPoint = mockSessionData[0];
    LogdataRow lastPoint = mockSessionData[mockSessionDataCount - 1];
    
    long realSessionStartMs = firstPoint.totalUptime;
    long realSessionEndMs = lastPoint.totalUptime;
    int realSessionDurationSeconds = (realSessionEndMs - realSessionStartMs) / 1000;
    
    // Add metadata
    JsonObject meta = doc["meta"].to<JsonObject>();
    meta["realStartTimestamp"] = realSessionStartMs;
    meta["realEndTimestamp"] = realSessionEndMs;
    meta["realDurationSeconds"] = realSessionDurationSeconds;
    meta["totalDatapoints"] = mockSessionDataCount;
    meta["chartDatapoints"] = mockSessionDataCount;
    meta["skipInterval"] = 1;
    
    // Add data array
    JsonArray data = doc["data"].to<JsonArray>();
    
    for (int i = 0; i < mockSessionDataCount; i++) {
        LogdataRow& row = mockSessionData[i];
        
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
    
    // Serialize to string
    String json;
    serializeJson(doc, json);
    
    return json;
}

String generateSessionCsvData(String sessionFile) {
    return "Total Uptime (s),Motor Temperature (degC)\\r\\n1.0,25.0\\r\\n";
} 