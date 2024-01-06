#include "datalog.h"
#include "log.h"
#include <FS.h>
#include <SPIFFS.h>
#include "motor.h"
#include "main.h"
/**
* Regularly saves data about the state of the vehicle to disc.
*/

/*
* CONSTANTS
*/ 

const String HEADER = "time,motor temp";
const String DATALOG_DIR = "/datalog";
const unsigned long DATALOG_INTERVAL = 1000;//how often we record a datapoint in milliseconds.

/*
* GLOBAL VARIABLES
*/ 
File csvFile;
unsigned long lastDataLogTime = millis();


void openCSVFile(){
  String filename;
  for(int i = 0; true; i++){
    filename=DATALOG_DIR+"/data_"+String(i)+".csv";
    if (!SPIFFS.exists(filename)) break;
  }
  csvFile = SPIFFS.open(filename, FILE_WRITE);
  if (EnableDebugLog) Serial.println(String("writing to "+filename));
  csvFile.println(HEADER);
  csvFile.flush();
}

void listLogFiles(){
  Serial.printf("Listing directory: %s\r\n", DATALOG_DIR);

  File root = SPIFFS.open(DATALOG_DIR);
  if(!root){
      Serial.println("- failed to open directory");
      return;
  }
  if(!root.isDirectory()){
      Serial.println(" - not a directory");
      return;
  }

  File file = root.openNextFile();
  while(file){
      if(file.isDirectory()){
          Serial.print("  DIR : ");
          Serial.println(file.name());
      } else {
          Serial.print("  FILE: ");
          Serial.print(file.name());
          Serial.print("\tSIZE: ");
          Serial.println(file.size());
      }
      file = root.openNextFile();
  }
}

LogdataRow createDatapoint(){
  LogdataRow dp;
  dp.time = millis();
  if (hasMotor){
    dp.tempMotor = getVescUart().data.tempMotor;
  }else{
    dp.tempMotor = 20.0+loopCount%10;
  }
  return dp;
}

void saveDatapoint(LogdataRow datapoint, File &file){
  file.print(datapoint.time);
  file.print(",");
  file.print(datapoint.tempMotor);
  file.println();
  file.flush();
}

void datalogSetup(){
  if(!SPIFFS.begin(true)){
      Serial.println("SPIFFS Mount Failed");
      return;
  }

  openCSVFile();

  listLogFiles();
}

void datalogLoop(){
  if (millis()>lastDataLogTime+DATALOG_INTERVAL){
    LogdataRow data = createDatapoint();
    saveDatapoint(data, csvFile);
    lastDataLogTime = millis();
  }
}
