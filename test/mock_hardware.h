#ifndef MOCK_HARDWARE_H
#define MOCK_HARDWARE_H

#include <string>

// Mock hardware variables
extern int LED_State;
extern int currentMotorStep;
extern bool remoteControlActive;
extern unsigned long lastActionTime;

// Mock hardware functions
void mock_setBarSpeed(int speed);
void mock_setLEDState(int state);
void mock_setBarLED(int level);
int mock_getTotalDataPoints(const std::string& timeRange);
std::string* mock_listSessionFiles(int* count);
std::string mock_getCurrentSessionFile();

#endif // MOCK_HARDWARE_H 