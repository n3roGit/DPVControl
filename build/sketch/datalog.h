#line 1 "C:\\Users\\christoph.bubeck\\Documents\\GitHub\\DPVControl\\DPVController\\datalog.h"

#ifndef datalog_h
#define datalog_h

struct LogdataRow {
  long time;
  float tempMotor;
};



void datalogSetup();
void datalogLoop();
void listLogFiles();


#endif