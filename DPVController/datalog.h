
#ifndef datalog_h
#define datalog_h

#include "Arduino.h" //For String

struct LogdataRow {
  long time;
  float motorTemp;
  float motorInpVoltage;
  float motorAvgInputCurrent;
  float motorAvgCurrent;
  float motorDutyCycleNow;
  float motorRpm;
  int motorState;
  float mosfetTemp;
  float chassisTemp;
  float chassisHumidity;
  int ledLampState;
  int speedSetting;
  int soc;
  int leakBack;
  int leakFront;
};

// Summary data structure for aggregated data
struct LogSummary {
  long startTime;
  long endTime;
  float avgMotorTemp;
  float avgMotorInpVoltage;
  float avgMotorCurrent;
  float avgMotorRpm;
  float avgMosfetTemp;
  float avgChassisTemp;
  float avgChassisHumidity;
  int minBatteryLevel;
  int maxBatteryLevel;
  int motorOnTime; // seconds
  int leakDetections;
};

void datalogSetup();
void datalogLoop();
void listLogFiles();
String readLogFile(String logname);
String createLogfilesHtml();
void deleteAllFiles();
void checkAndCleanupOldFiles();
size_t getTotalLogSize();
void createSummaryFile(String oldestFile, String secondOldestFile);

#endif