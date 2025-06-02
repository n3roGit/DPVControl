#include "mock_webserver.h"
#include "mock_hardware.h"
#include <ArduinoJson.h>
#include <string>

extern std::string mockResponse;
extern bool mockRebootCalled;

void handleClient(class MockClient& client) {
    // Mock implementation of handleClient
    // This simulates the webserver's response handling
    
    // For status API
    JsonDocument doc;
    doc["uptime"] = 1000;
    doc["totalUptime"] = 1000;
    doc["dataPoints"] = 100;
    doc["beeperEnabled"] = true;
    doc["lampLevel"] = LED_State;
    doc["waterSensorFront"] = false;
    doc["waterSensorBack"] = false;
    doc["leftButton"] = false;
    doc["rightButton"] = false;
    
    std::string jsonString;
    serializeJson(doc, jsonString);
    mockResponse = jsonString;
}

void sendHttpResponse(class MockClient& client, int statusCode, const char* contentType, const char* content) {
    // Mock implementation of sendHttpResponse
    mockResponse = content;
} 