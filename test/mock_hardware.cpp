#include "mock_hardware.h"
#include <string>

// Mock hardware variables
int LED_State = 0;
int currentMotorStep = 0;
bool remoteControlActive = false;
unsigned long lastActionTime = 0;

// Mock hardware functions
void mock_setBarSpeed(int speed) {
    // Mock implementation
}

void mock_setLEDState(int state) {
    LED_State = state;
}

void mock_setBarLED(int level) {
    // Mock implementation
}

int mock_getTotalDataPoints(const std::string& timeRange) {
    return 100; // Mock value
}

std::string* mock_listSessionFiles(int* count) {
    *count = 2;
    std::string* files = new std::string[2];
    files[0] = "session1.csv";
    files[1] = "session2.csv";
    return files;
}

std::string mock_getCurrentSessionFile() {
    return "current_session.csv";
} 