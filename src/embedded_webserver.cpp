#include "embedded_webserver.h"
#include "log.h"
#include <Arduino.h>

// Include the embedded files registry (will be generated)
#ifdef __has_include
  #if __has_include("generated/embedded_files_registry.h")
    #include "generated/embedded_files_registry.h"
    #define HAS_EMBEDDED_FILES
  #endif
#endif

bool serveEmbeddedFile(WiFiClient client, const char* path) {
    #ifdef HAS_EMBEDDED_FILES
    // Find the embedded file
    const EmbeddedFile* file = findEmbeddedFile(path);
    
    if (file == nullptr) {
        String logMsg = "Embedded file not found: " + String(path);
        log(logMsg.c_str());
        return false;
    }
    
    // Log the file being served
    String logMsg = "Serving embedded file: " + String(file->filename) + " (" + String(file->size) + " bytes)";
    log(logMsg.c_str());
    
    // Send the response
    sendEmbeddedResponse(client, 200, file->content_type, file->data, file->size, true);
    
    return true;
    #else
    // No embedded files available
    log("No embedded files registry available");
    return false;
    #endif
}

void sendEmbeddedResponse(WiFiClient client, int statusCode, const char* contentType, 
                         const uint8_t* data, size_t dataSize, bool isProgmem) {
    // Send HTTP response header
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
    client.print("Content-Length: ");
    client.println(dataSize);
    client.println("Connection: close");
    
    // Add cache headers for static files
    if (strstr(contentType, "text/html") == nullptr) {
        client.println("Cache-Control: public, max-age=3600"); // 1 hour cache for non-HTML files
    } else {
        client.println("Cache-Control: no-cache"); // No cache for HTML files
    }
    
    client.println(); // End of headers
    
    // Send the content
    if (isProgmem) {
        // Read from PROGMEM in chunks
        const size_t chunkSize = 512;
        size_t remaining = dataSize;
        size_t offset = 0;
        
        while (remaining > 0 && client.connected()) {
            size_t toRead = (remaining > chunkSize) ? chunkSize : remaining;
            
            // Read chunk from PROGMEM
            uint8_t buffer[chunkSize];
            memcpy_P(buffer, data + offset, toRead);
            
            // Send chunk to client
            client.write(buffer, toRead);
            
            offset += toRead;
            remaining -= toRead;
            
            // Small delay to prevent watchdog issues
            if (remaining > 0) {
                yield();
            }
        }
    } else {
        // Send directly from RAM
        client.write(data, dataSize);
    }
} 