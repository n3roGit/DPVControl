#line 1 "C:\\Users\\christoph.bubeck\\Documents\\GitHub\\DPVControl\\DPVController\\log.h"
#ifndef log_h
#define log_h

/**
*
* Code that mostly just logs information to serial.
*
*/ 

const bool EnableDebugLog = true; //Enable/Disable Serial Log

void log(const char* label, int value, bool doLog);
void log(const char* label, int value);
void log(const char* label);
void logVehicleState();

#endif