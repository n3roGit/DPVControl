#ifndef log_h
#define log_h

#include "settings.h"

/**
*
* Code that mostly just logs information to serial.
*
*/ 

// Log levels: 0 = Error, 1 = Info, 2 = Debug
// Backward compatibility: EnableDebugLog == (logLevel >= Debug)
#define EnableDebugLog getDebugLoggingEnabled()

void log(const char* label, int value, bool doLog);
void log(const char* label, int value);
void log(const char* label);
void log(const String& message);  // Add String support
void logDebug(const char* label);
void logDebug(const String& message);
void logError(const char* label);
void logError(const String& message);
void logVehicleState();

#endif
