#ifndef all_h
#define all_h
#include <VescUart.h>
#include "DHTesp.h"

//temporary collection of header definitions 
//that should be moved to their own places.

//VARIABLES
extern int leakSensorState;

extern int loopCount;
extern int NormalLogOutputIntervall;


extern DHTesp dhtSensor;


//FUNCTIONS
VescUart getVescUart();



#endif