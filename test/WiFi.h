#pragma once
#ifndef WIFI_H
#define WIFI_H

// Dummy WiFi header for compatibility

class WiFiClass {
public:
    static int status() { return 0; }
    static void begin(const char*, const char*) {}
    static void disconnect() {}
};

static WiFiClass WiFi;

#endif // WIFI_H 