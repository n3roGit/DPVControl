#ifndef DATA_UPLOAD_H
#define DATA_UPLOAD_H

#include <SPIFFS.h>

// Function to initialize SPIFFS and store HTML files
bool initializeFileSystem();

// Function to store a file in SPIFFS
bool storeFile(const char* path, const char* content);

#endif // DATA_UPLOAD_H 