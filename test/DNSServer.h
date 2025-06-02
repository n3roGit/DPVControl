#pragma once

class DNSServer {
public:
    void start(int port, const char* domainName, const char* ip) {}
    void stop() {}
    void processNextRequest() {}
}; 