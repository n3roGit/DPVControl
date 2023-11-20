#include "all.h"

int leakSensorState = 0;
MotorState motorState = standby;

int loopCount = 0;
int NormalLogOutputIntervall = 1000*10;

//Battery
int batteryLevel = 0;// 0 to 100% state of charge. 

bool hasMotor = true;//Indicates that we have an actual motor plugged in.
