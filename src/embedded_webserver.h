#ifndef EMBEDDED_WEBSERVER_H
#define EMBEDDED_WEBSERVER_H

#include <WiFiClient.h>

// Function to serve embedded files instead of SPIFFS files
bool serveEmbeddedFile(WiFiClient client, const char* path);

// Helper function to send HTTP response with embedded content
void sendEmbeddedResponse(WiFiClient client, int statusCode, const char* contentType, 
                         const uint8_t* data, size_t dataSize, bool isProgmem = true);

#endif // EMBEDDED_WEBSERVER_H 