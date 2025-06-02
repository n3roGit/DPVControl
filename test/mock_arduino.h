#ifndef MOCK_ARDUINO_H
#define MOCK_ARDUINO_H

#include <cstdint>
#include <string>
#include <cstring>

// Basic Arduino types
typedef uint8_t byte;
typedef bool boolean;

// Mock Arduino functions
unsigned long millis();
void delay(unsigned long ms);
void digitalWrite(uint8_t pin, uint8_t value);
int digitalRead(uint8_t pin);
void pinMode(uint8_t pin, uint8_t mode);

// Mock Serial
class SerialClass {
public:
    void begin(unsigned long baud) {}
    void print(const char* str) {}
    void println(const char* str) {}
    void println(int val) {}
    void println(unsigned long val) {}
    void println(float val) {}
    void println() {}
};

extern SerialClass Serial;

// Mock String class
class String {
public:
    String() {}
    String(const char* str) : str_(str) {}
    String(const String& other) : str_(other.str_) {}
    
    String& operator=(const String& other) {
        str_ = other.str_;
        return *this;
    }
    
    const char* c_str() const { return str_.c_str(); }
    int length() const { return str_.length(); }
    bool isEmpty() const { return str_.empty(); }
    
    String substring(int start, int end = -1) const {
        if (end == -1) end = str_.length();
        return String(str_.substr(start, end - start).c_str());
    }
    
    int indexOf(char c) const {
        size_t pos = str_.find(c);
        return pos == std::string::npos ? -1 : pos;
    }
    
    int toInt() const {
        return std::stoi(str_);
    }
    
private:
    std::string str_;
};

#endif // MOCK_ARDUINO_H 