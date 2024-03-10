#include "datalog.h"
#include "log.h"
#include <FS.h>
#include <SPIFFS.h>
#include "string.h"
#include "motor.h"
#include "main.h"
#include "dht.h"
#include "Arduino.h" //For String
/**
* Regularly saves data about the state of the vehicle to disc.
*/

/*
* CONSTANTS
*/ 

const String HEADER = "time,motor temp,motor input voltage,motor average input current,"
"motor average current,motor duty cycle,motor RPM,mosfet temp,"
"chassis temp,chassis humidity";
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

String createLogfilesHtml(){
  String html = "<html><body>\r\n";

  File root = SPIFFS.open(DATALOG_DIR);
  if(!root){
      return "- failed to open directory";
  }
  if(!root.isDirectory()){
      return " - not a directory";
  }

  File file = root.openNextFile();
  while(file){
      if(!file.isDirectory()){
        html += "<a href=\"logs?log=";
        html += file.name();
        html += "\">";
        html += file.name();
        html += "</a><br />";
        html += "\r\n";
      } 
      file = root.openNextFile();
  }
  html += "</body></html>";
  return html;
}

String readLogFile(String logname){
  File file = SPIFFS.open(DATALOG_DIR+"/"+logname);
  if (!file.available()){
    return "can not read " + logname;
  }
  return file.readString();
}

LogdataRow createDatapoint(){
  LogdataRow dp;
  dp.time = millis();
  if (HAS_MOTOR){
    dp.motorTemp = getVescUart().data.tempMotor;
    dp.motorInpVoltage = getVescUart().data.inpVoltage;
    dp.motorAvgInputCurrent = getVescUart().data.avgInputCurrent;
    dp.motorAvgCurrent = getVescUart().data.avgMotorCurrent;
    dp.motorDutyCycleNow = getVescUart().data.dutyCycleNow;
    dp.motorRpm = getVescUart().data.rpm;
    dp.mosfetTemp = getVescUart().data.tempMosfet;
  }else{
    dp.motorTemp = 20.0+loopCount%10;
    dp.motorInpVoltage = 20.0+loopCount%11;
    dp.motorAvgInputCurrent = 20.0+loopCount%12;
    dp.motorAvgCurrent = 20.0+loopCount%13;
    dp.motorDutyCycleNow = 20.0+loopCount%14;
    dp.motorRpm = 20.0+loopCount%15;
    dp.mosfetTemp = 20.0+loopCount%16;    
  }
  dp.chassisHumidity = getHuminity();
  dp.chassisTemp = getTemp();
  return dp;
}

void saveDatapoint(LogdataRow datapoint, File &file){
  file.print(datapoint.time);
  file.print(",");
  file.print(datapoint.motorTemp);
  file.print(",");
  file.print(datapoint.motorInpVoltage);
  file.print(",");
  file.print(datapoint.motorAvgInputCurrent);
  file.print(",");
  file.print(datapoint.motorAvgCurrent);
  file.print(",");
  file.print(datapoint.motorDutyCycleNow);
  file.print(",");      
  file.print(datapoint.motorRpm);
  file.print(",");    
  file.print(datapoint.mosfetTemp);
  file.print(",");             
  file.print(datapoint.chassisTemp);
  file.print(",");
  file.print(datapoint.chassisHumidity);
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
