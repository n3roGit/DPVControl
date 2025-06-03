#pragma once

#include <VescUart.h>
#include <Arduino.h>

// Motor states
enum MotorState {
    standby,
    on,
    off,
    cruise,
    turbo,
    jammed
};

// External variables
extern MotorState motorState;
extern bool remoteControlActive; // Flag for remote control override
extern int currentMotorStep; // Current speed step (1 to speedSteps)
extern unsigned long lastActionTime;

extern const bool HAS_MOTOR;//Indicates that we have an actual motor plugged in.

VescUart& getVescUart();//Accessor

// Motor control functions
void setMotorSpeed(int speed);
void updateMotorState();
int getSpeedSteps();
int getStandbyDelay();
int getMaxTimeOverloaded();

void motorSetup();
void motorLoop();
void speedUp();
void speedDown();
void wakeUp();
void standBy();
void enterCruiseMode();
void leaveCruiseMode();
void enterTurboMode();
void leaveTurboMode();