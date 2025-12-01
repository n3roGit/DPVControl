#ifndef MOCK_WEBSERVER_H
#define MOCK_WEBSERVER_H

#include "mock_arduino.h"
#include "mock_datalog.h"

extern String mockResponse;
extern bool mockRebootCalled;

extern "C" {
    void handleApiStatus();
    void handleApiMotor();
    void handleApiLamp();
    void handleApiBeeper();
    void handleApiSettings();
    void handleApiVersion();
    void handleApiVescBridge();
}

// Mock session data generation functions for testing
String generateSessionDataJson(String sessionFile);
String generateSessionCsvData(String sessionFile);

// Mock helper functions
void sendHttpResponse(const char* response);

#endif 