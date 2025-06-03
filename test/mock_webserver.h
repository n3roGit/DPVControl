#pragma once
#include <string>

extern std::string mockResponse;
extern bool mockRebootCalled;

void handleClient();
void sendHttpResponse(const char* response);

extern "C" {
    void handleApiStatus();
    void handleApiMotor();
    void handleApiLamp();
    void handleApiBeeper();
    void handleApiSettings();
    void handleApiVersion();
}

// Mock webserver functions
void handleClient(class MockClient& client);
void sendHttpResponse(class MockClient& client, int statusCode, const char* contentType, const char* content); 