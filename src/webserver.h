#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <WiFi.h>
#include <DNSServer.h>
#include <WiFiServer.h>
#include <WiFiClient.h>
// Note: Using LittleFS instead of SPIFFS
#include "main.h"

// HTTP Request structure for cleaner request handling
struct HttpRequest {
    String method;
    String path;
    String host;
    String contentLength;
    bool isCaptivePortalRequest;
    
    // Constructor
    HttpRequest() : isCaptivePortalRequest(false) {}
};

// Task handle for the webserver task
extern TaskHandle_t webserverTaskHandle;

// WiFi Credentials
extern const char* ssid;
extern const char* password;

// DNS and WebServer objects
extern DNSServer dnsServer;
extern WiFiServer server;

// Function declarations
void setupWebserver();
void webserverTask(void *pvParameters);
void handleClient(WiFiClient client);
bool loadFromSPIFFS(WiFiClient client, String path);
void sendHttpResponse(WiFiClient client, int statusCode, const char* contentType, const char* content);
HttpRequest parseHttpRequest(WiFiClient& client);

// API handler functions
void handleApiStatus(WiFiClient& client);
void handleApiMotor(WiFiClient& client, const String& contentLength);
void handleApiLamp(WiFiClient& client, const String& contentLength);
void handleApiData(WiFiClient& client, const String& path);
void handleApiSettingsGet(WiFiClient& client);
void handleApiSettingsPost(WiFiClient& client, const String& contentLength);

// Static file handler functions
void handleStaticHtmlFile(WiFiClient& client, const String& path, const String& filename);
void handleLargeJsFile(WiFiClient& client, const String& filename);
void handleCaptivePortalDetection(WiFiClient& client);
void handleGenericStaticFile(WiFiClient& client, const String& path, bool isCaptivePortalRequest);

#endif // WEBSERVER_H 