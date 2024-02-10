
#ifndef datalog_h
#define datalog_h

struct LogdataRow {
  long time;
  float tempMotor;
  float chassisTemp;
  float chassisHumidity;
};



void datalogSetup();
void datalogLoop();
void listLogFiles();


#endif