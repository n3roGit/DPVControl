#ifndef log_h
#define log_h

#include "settings.h"

/**
*
* Code that mostly just logs information to serial.
*
*/ 

// Use settings-based debug logging
#define EnableDebugLog getDebugLoggingEnabled()

void log(const char* label, int value, bool doLog);
void log(const char* label, int value);
void log(const char* label);
void log(const String& message);  // Add String support
void logVehicleState();

#endif