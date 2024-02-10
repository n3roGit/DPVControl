
#ifndef datalog_h
#define datalog_h

#include "Arduino.h" //For String

struct LogdataRow {
  long time;
  float tempMotor;
  float chassisTemp;
  float chassisHumidity;
};



void datalogSetup();
void datalogLoop();
void listLogFiles();
String createLogfilesHtml();


#endif