#pragma once

class WiFiClient {
public:
    WiFiClient() {}
    bool connected() { return false; }
    void stop() {}
    int available() { return 0; }
    int read() { return -1; }
    int read(uint8_t* buf, size_t size) { return 0; }
    int write(uint8_t b) { return 0; }
    int write(const uint8_t* buf, size_t size) { return 0; }
    int peek() { return -1; }
    void flush() {}
}; 