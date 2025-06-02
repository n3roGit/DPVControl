#ifndef MOCK_SPIFFS_H
#define MOCK_SPIFFS_H

#include <string>
#include <map>

class SPIFFSClass {
public:
    bool begin(bool formatOnFail = false) { return true; }
    bool exists(const char* path) { return true; }
    bool remove(const char* path) { return true; }
    bool format() { return true; }
};

extern SPIFFSClass SPIFFS;

#endif // MOCK_SPIFFS_H 