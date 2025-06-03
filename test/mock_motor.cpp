#include "motor.h"

// Mocked motor state variables
MotorState motorState = off;
bool remoteControlActive = false;
int currentMotorStep = 0;
unsigned long lastActionTime = 0;

// Mocked settings
int mockSpeedSteps = 10;
int mockStandbyDelay = 60; // seconds
int mockMaxTimeOverloaded = 5000; // ms

void setMotorSpeed(int speed) {
    int maxSteps = getSpeedSteps();
    currentMotorStep = (speed * maxSteps) / 100;
    if (speed > 0) {
        motorState = on;
    } else {
        motorState = off;
    }
    lastActionTime = micros();
}

void updateMotorState() {
    if (motorState == on && !remoteControlActive) {
        unsigned long currentTime = micros();
        if (currentTime - lastActionTime > getStandbyDelay() * 1000000) {
            motorState = standby;
        }
    }
}

int getSpeedSteps() {
    return mockSpeedSteps;
}

int getStandbyDelay() {
    return mockStandbyDelay;
}

int getMaxTimeOverloaded() {
    return mockMaxTimeOverloaded;
} 