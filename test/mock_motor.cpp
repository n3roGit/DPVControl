#include "motor.h"

// Mocked motor state variables
MotorState motorState = off;
bool remoteControlActive = false;
extern int currentMotorStep;
extern unsigned long lastActionTime;
extern bool motorOverload;

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
    unsigned long currentTime = micros();
    
    // Check for overload condition
    if (motorOverload && (currentTime - lastActionTime > getMaxTimeOverloaded() * 1000)) {
        motorState = off;
        currentMotorStep = 0;
        return;
    }
    
    // Check for standby condition
    if (motorState == on && !remoteControlActive) {
        if (currentTime - lastActionTime > getStandbyDelay() * 1000) { // ms statt us
            motorState = standby;
            currentMotorStep = 0;
        }
    }
}

int getSpeedSteps() {
    return mockSpeedSteps;
}

int getStandbyDelay() {
    return mockStandbyDelay;
}

long getMaxTimeOverloaded() {
    return mockMaxTimeOverloaded;
} 