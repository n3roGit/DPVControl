
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
  float mosfetTemp;
  float chassisTemp;
  float chassisHumidity;
  int ledLampState;
};



void datalogSetup();
void datalogLoop();
void listLogFiles();
String readLogFile(String logname);
String createLogfilesHtml();
void deleteAllFiles();


#endif