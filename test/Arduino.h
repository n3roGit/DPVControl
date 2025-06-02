#pragma once
#include "mock_arduino.h"

// ArduinoJson compatibility shims for native test
#ifndef PROGMEM
#define PROGMEM
#endif

#ifndef F
#define F(x) x
#endif

// Dummy __FlashStringHelper
typedef char __FlashStringHelper;

// Dummy pgm_read_byte
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#define pgm_read_word(addr) (*(const unsigned short *)(addr))
#define pgm_read_dword(addr) (*(const unsigned long *)(addr))
#define PSTR(x) (x)

// Dummy Print/Stream base classes
class Print {
public:
    size_t write(uint8_t) { return 1; }
    size_t write(const char*) { return 1; }
    size_t write(const uint8_t*, size_t) { return 1; }
    size_t print(const char*) { return 1; }
    size_t println(const char*) { return 1; }
    size_t printTo(Print&) { return 1; }
};

class Stream : public Print {
public:
    int available() { return 0; }
    int read() { return -1; }
    int peek() { return -1; }
    void flush() {}
    size_t readBytes(char* buffer, size_t length) { return 0; }
};

// Dummy readBytes für Stream
inline size_t readBytes(char* buffer, size_t length) { return 0; }

// WiFi dummy
class WiFiClass {
public:
    static void begin(const char* ssid, const char* password) {}
    static void disconnect() {}
    static int status() { return 3; } // WL_CONNECTED
    static const char* SSID() { return "test_ssid"; }
    static const char* localIP() { return "192.168.1.1"; }
}; 