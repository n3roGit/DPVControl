
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
};



void datalogSetup();
void datalogLoop();
void listLogFiles();
String readLogFile(String logname);
String createLogfilesHtml();
void deleteAllFiles();


#endif