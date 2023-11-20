#ifndef all_h
#define all_h
#include <VescUart.h>
#include "DHTesp.h"

//temporary collection of header definitions 
//that should be moved to their own places.


enum MotorState {standby, on, off, cruise, turbo};

//VARIABLES
extern int leakSensorState;
extern MotorState motorState;

extern int loopCount;
extern int NormalLogOutputIntervall;

//Battery
extern int batteryLevel;// 0 to 100% state of charge. 

extern DHTesp dhtSensor;

extern bool hasMotor;//Indicates that we have an actual motor plugged in.


//FUNCTIONS
VescUart getVescUart();



#endif