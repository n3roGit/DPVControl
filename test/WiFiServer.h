#pragma once

class WiFiServer {
public:
    WiFiServer(int port) {}
    void begin() {}
    void stop() {}
    bool hasClient() { return false; }
    void close() {}
}; 