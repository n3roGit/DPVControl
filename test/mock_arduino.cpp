#include "mock_arduino.h"
#include <chrono>

static auto start_time = std::chrono::steady_clock::now();

void delay(unsigned long ms) {
    // No-op in tests
}

void digitalWrite(uint8_t pin, uint8_t value) {
    // No-op in tests
}

int digitalRead(uint8_t pin) {
    return 0; // Default to LOW
}

void pinMode(uint8_t pin, uint8_t mode) {
    // No-op in tests
}

SerialClass Serial; 