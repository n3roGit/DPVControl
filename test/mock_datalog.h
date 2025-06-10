#ifndef MOCK_DATALOG_H
#define MOCK_DATALOG_H

#include <stdint.h>

// Mock LogdataRow structure (copy from src/datalog.h but without ESP32 dependencies)
struct LogdataRow {
  long timestamp;
  float tempMotor;
  float tempMosfet;
  float batteryVoltage;
  float current;
  float avgMotorCurrent;
  float erpm;
  float dutyCycle;
  float temperature;
  float humidity;
  int batteryLevel;
  int leakSensorState;
  int ledBrightness;
  int leftButton;
  int rightButton;
  int beeperEnabled;
  int beeperActive;
  unsigned long totalUptime;
};

// Mock global variables
extern unsigned long bootTimeSeconds;

// Mock functions that would normally be in datalog.cpp
unsigned long getTotalUptime();
LogdataRow createOptimizedDatapoint(unsigned long currentTime);
bool isValidRuntime(unsigned long runtime);

#endif 