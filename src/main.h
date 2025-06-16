#ifndef all_h
#define all_h
#include <VescUart.h>
#include "DHTesp.h"

// Include FreeRTOS for multi-core functionality
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

//temporary collection of header definitions 
//that should be moved to their own places.

//VARIABLES
extern int leakSensorFront;        // Current front leak sensor state (0=OK, 1=LEAK)
extern int leakSensorBack;         // Current back leak sensor state (0=OK, 1=LEAK)
extern int leakAlarmPersistent;    // Persistent leak alarm flag (survives reboots, 0=OK, 1=ALARM)
extern int leakAlarmFrontPersistent; // Persistent front sensor alarm (0=OK, 1=FRONT_ALARM)
extern int leakAlarmBackPersistent;  // Persistent back sensor alarm (0=OK, 1=BACK_ALARM)
extern int loopCount;
extern int NormalLogOutputIntervall;
extern DHTesp dhtSensor;

#endif