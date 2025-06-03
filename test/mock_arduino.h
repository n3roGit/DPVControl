#pragma once
#ifndef MOCK_ARDUINO_H
#define MOCK_ARDUINO_H

#include "mock_print.h"
#include <cstdint>
#include <string>
#include <cstring>
#include <chrono>

// Basic Arduino types
typedef uint8_t byte;
typedef bool boolean;

// Comfort states
enum ComfortState {
    Comfort_TooHot,
    Comfort_TooCold,
    Comfort_OK
};

enum PerceptionState {
    Perception_Cold,
    Perception_Hot,
    Perception_Dry,
    Perception_Wet,
    Perception_Comfy,
    Perception_UnComfy
};

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
class String : public Printable {
public:
    String() {}
    String(const char* str) : str_(str ? str : "") {}
    String(const String& other) : str_(other.str_) {}
    
    String& operator=(const String& other) {
        str_ = other.str_;
        return *this;
    }
    String& operator=(const char* str) {
        str_ = str ? str : "";
        return *this;
    }
    
    String& operator+=(const String& other) {
        str_ += other.str_;
        return *this;
    }
    
    String& operator+=(const char* str) {
        str_ += str;
        return *this;
    }
    
    friend String operator+(const String& lhs, const String& rhs) {
        String result(lhs);
        result += rhs;
        return result;
    }
    
    friend String operator+(const String& lhs, const char* rhs) {
        String result(lhs);
        result += rhs;
        return result;
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
    
    bool concat(const char* str) { return true; }
    bool concat(const String& str) { return true; }

    // Implement Printable interface
    size_t printTo(Print& p) const override {
        return p.write(reinterpret_cast<const uint8_t*>(str_.c_str()), str_.length());
    }

private:
    std::string str_;
};

// millis() Ersatz
inline unsigned long millis() {
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return (unsigned long)std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
}

#endif // MOCK_ARDUINO_H 