#ifndef EMBEDDED_WEBSERVER_H
#define EMBEDDED_WEBSERVER_H

#include <WiFiClient.h>

// Include embedded files registry if available
#ifdef __has_include
  #if __has_include("generated/embedded_files_registry.h")
    #include "generated/embedded_files_registry.h"
    #define HAS_EMBEDDED_FILES
  #endif
#endif

// If embedded files registry is not available, define fallback types
#ifndef HAS_EMBEDDED_FILES
struct EmbeddedFile {
    const char* filename;
    const char* content_type;
    const uint8_t* data;
    size_t size;
    bool is_binary;
    uint32_t checksum;
};

// Fallback function that always returns nullptr
inline const EmbeddedFile* findEmbeddedFile(const char* path) {
    return nullptr;
}
#endif

// Function to serve embedded files instead of SPIFFS files
bool serveEmbeddedFile(WiFiClient client, const char* path);

// Helper function to send HTTP response with embedded content
void sendEmbeddedResponse(WiFiClient client, int statusCode, const char* contentType, 
                         const uint8_t* data, size_t dataSize, bool isProgmem = true);

#endif // EMBEDDED_WEBSERVER_H 