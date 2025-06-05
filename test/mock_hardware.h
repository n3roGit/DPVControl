#ifndef MOCK_HARDWARE_H
#define MOCK_HARDWARE_H

#include <string>
#include <array>
#include <mutex>
#include <cstdint>
#include "mock_arduino.h"
#include <vector>

// Mock hardware variables
extern int LED_State;
extern int currentMotorStep;
extern bool remoteControlActive;
extern unsigned long lastActionTime;
extern unsigned long bootTimeSeconds; // For uptime testing
extern std::array<bool, 20> mockLEDBarStates;
extern std::array<uint32_t, 20> mockLEDBarColors;
extern int mockLEDBarBrightness;
extern int mockLEDBarBrightnessSecond;
extern bool mockLEDUpdateInProgress;
extern int mockLastDisplayedSpeed;
extern int mockLastDisplayedMotorState;
extern int mockLastDisplayedBattery;
extern bool mockWaterSensorFront;
extern bool mockWaterSensorBack;
extern float mockTemperature;
extern float mockHumidity;
extern bool mockDHTValid;
extern bool mockDHTError;
extern bool mockDHTTimeout;
extern bool mockLedState;
extern int mockMotorSteps;
extern std::vector<std::string> mockInvalidInputs;

// Mock hardware functions
void mock_ledBarSetup();
void mock_setBarSpeed(int speed);
void mock_setLEDState(int state);
void mock_setBarLED(int level);
void mock_setBarStandby();
void mock_setBarSpeedCruise(int num);
void mock_setBarBattery(int num);
void mock_setBarLeak();
void mock_setBarPowerBank(bool status);
void mock_setBarFlasher(bool status);
void mock_forceRefreshLedBar();
void mock_knightRiderStartup();
void mock_setWaterSensorFront(bool state);
void mock_setWaterSensorBack(bool state);
bool mock_getWaterSensorFront();
bool mock_getWaterSensorBack();
void mock_setTemperature(float temp);
void mock_setHumidity(float hum);
float mock_getTemperature();
float mock_getHumidity();
void mock_setDHTError(bool error);
void mock_setDHTTimeout(bool timeout);
bool mock_getDHTValid();
int mock_getTotalDataPoints(const std::string& timeRange);
std::string* mock_listSessionFiles(int* count);
std::string mock_getCurrentSessionFile();
void simulateOverload(bool overload);
unsigned long micros();
void advanceTime(unsigned long ms);
void resetMockTime();

// Helper functions
uint32_t mock_color(uint8_t r, uint8_t g, uint8_t b);
void mock_getStripBoundaries(int stripNumber, int& startIndex, int& endIndex);
bool mock_safeSetPixelColor(int index, uint32_t color);
int mock_calculateBrightnessCorrectedValue(int red, int green, int blue, int targetBrightness);
void setMockMillis(unsigned long ms); // For testing uptime calculations

// DHT22 calculation functions
float mock_computeHeatIndex(float temperature, float humidity);
float mock_computeDewPoint(float temperature, float humidity);
float mock_getComfortRatio(ComfortState& comfort, float temperature, float humidity);
uint8_t mock_computePerception(float temperature, float humidity);
float mock_computeAbsoluteHumidity(float temperature, float humidity);

// Rename functions to match usage
void mock_resetAllStates();
std::string mockHandleApiStatus();
bool mockIsInputValid(const std::string& input);

#endif 