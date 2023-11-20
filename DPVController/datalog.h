
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