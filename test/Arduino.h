#define ARDUINOJSON_ENABLE_PROGMEM 0
#define ARDUINOJSON_ENABLE_FLASH_STRING 0
#pragma once

#include "mock_print.h"
#include "mock_arduino.h"
#include "mock_webserver.h"
#include "mock_hardware.h"
#include "mock_spiffs.h"

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

#include <ArduinoJson.h>

// WiFi dummy
class WiFiClass {
public:
    static void begin(const char* ssid, const char* password) {}
    static void disconnect() {}
    static int status() { return 3; } // WL_CONNECTED
    static const char* SSID() { return "test_ssid"; }
    static const char* localIP() { return "192.168.1.1"; }
}; 