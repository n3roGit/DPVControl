#ifndef motor_h
#define motor_h

#include <VescUart.h>

enum MotorState {standby, on, off, cruise, turbo, jammed};

extern MotorState motorState;

extern const bool HAS_MOTOR;//Indicates that we have an actual motor plugged in.

VescUart& getVescUart();//Accessor

void motorSetup();
void motorLoop();
void speedUp();
void speedDown();
int getSpeedSetting();
void wakeUp();
void standBy();
void enterCruiseMode();
void leaveCruiseMode();
void enterTurboMode();
void leaveTurboMode();


#endif