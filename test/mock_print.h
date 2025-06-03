#pragma once
#include <cstddef>
#include <cstdint>

class Print {
public:
    size_t write(uint8_t) { return 1; }
    size_t write(const char*) { return 1; }
    size_t write(const uint8_t*, size_t) { return 1; }
    size_t print(const char*) { return 1; }
    size_t println(const char*) { return 1; }
    size_t printTo(Print&) { return 1; }
};

class Printable {
public:
    virtual size_t printTo(Print& p) const = 0;
};

class Stream : public Print {
public:
    int available() { return 0; }
    int read() { return -1; }
    int peek() { return -1; }
    void flush() {}
    size_t readBytes(char* buffer, size_t length) { return 0; }
};

inline size_t readBytes(char* buffer, size_t length) { return 0; } 