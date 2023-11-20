#ifndef other_h
#define other_h
#include "all.h"
#include "constants.h"
#include "beep.h"
#include "log.h"
#include "motor.h"
#include "battery.h"
#include "ledBar.h"
#include "ledLamp.h"

/**
* For code that I did not find a better place for.
*/

void GetVESCValues();
void checkForLeak();
void FromTimeToTimeExecution(); 

#endif