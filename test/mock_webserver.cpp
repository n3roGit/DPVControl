#define ARDUINOJSON_ENABLE_PROGMEM 0
#define ARDUINOJSON_ENABLE_FLASH_STRING 0
#include "mock_webserver.h"
#include "mock_hardware.h"
#include "ArduinoJson_config.h"
#include "ArduinoJson.h"
#include <string>

std::string mockResponse;
extern bool mockRebootCalled;

void handleClient() {
    // Dummy implementation
}

void sendHttpResponse(const char* response) {
    mockResponse = response ? response : "";
}

// Dummy API handler implementations for native tests
extern "C" {
    void handleApiStatus() {
        JsonDocument doc;
        doc["status"] = "ok";
        doc["uptime"] = mockMillisValue; // Current uptime in ms
        doc["totalUptime"] = (mockMillisValue / 1000) + bootTimeSeconds; // Total uptime in seconds
        doc["motor"] = false;
        doc["lamp"] = false;
        doc["beeper"] = false;
        doc["erpm"] = 0.0;
        doc["leftButton"] = false;
        doc["rightButton"] = false;
        std::string response;
        serializeJson(doc, response);
        sendHttpResponse(response.c_str());
    }

    void handleApiMotor() {
        JsonDocument doc;
        doc["success"] = true;
        std::string response;
        serializeJson(doc, response);
        sendHttpResponse(response.c_str());
    }

    void handleApiLamp() {
        JsonDocument doc;
        doc["success"] = true;
        std::string response;
        serializeJson(doc, response);
        sendHttpResponse(response.c_str());
    }

    void handleApiBeeper() {
        JsonDocument doc;
        doc["success"] = true;
        std::string response;
        serializeJson(doc, response);
        sendHttpResponse(response.c_str());
    }

    void handleApiSettings() {
        JsonDocument doc;
        doc["motorEnabled"] = true;
        doc["lampEnabled"] = true;
        doc["beeperEnabled"] = true;
        doc["motorDuration"] = 1000;
        doc["lampDuration"] = 1000;
        doc["beeperDuration"] = 1000;
        std::string response;
        serializeJson(doc, response);
        sendHttpResponse(response.c_str());
    }

    void handleApiVersion() {
        JsonDocument doc;
        doc["version"] = "1.0.0";
        std::string response;
        serializeJson(doc, response);
        sendHttpResponse(response.c_str());
    }
} 