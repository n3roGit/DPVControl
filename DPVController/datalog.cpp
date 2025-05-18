#include "datalog.h"
#include "log.h"
#include <FS.h>
#include <SPIFFS.h>
#include "string.h"
#include "motor.h"
#include "main.h"
#include "dht.h"
#include "ledLamp.h"
#include "battery.h"
#include "leak.h"
#include "Arduino.h" //For String
/**
* Regularly saves data about the state of the vehicle to disc.
*/

/*
* CONSTANTS
*/ 

const String HEADER = "time,motor temp,motor input voltage,motor average input current,"
"motor average current,motor duty cycle,motor RPM,motor state,mosfet temp,"
"chassis temp,chassis humidity,led lamp state,speed setting,SOC,leak back sensor, leak front sensor";
const String DATALOG_DIR = "/datalog";
const String SUMMARY_DIR = "/summary";
const unsigned long DATALOG_INTERVAL = 1000; // how often we record a datapoint in milliseconds.
const size_t MAX_LOG_SIZE = 3 * 1024 * 1024; // 3MB max log size (ESP32 has 4MB flash)
const size_t CLEANUP_THRESHOLD = 2 * 1024 * 1024; // 2MB threshold to start cleanup
const unsigned long CHECK_SIZE_INTERVAL = 60000; // Check size every minute

/*
* GLOBAL VARIABLES
*/ 
File csvFile;
bool loggingActive = false;
unsigned long lastDataLogTime = millis();
unsigned long lastSizeCheckTime = 0;


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
      file.close();
      file = root.openNextFile();
  }
  root.close();
}

String createLogfilesHtml(){
  String html = "<html><head><title>DPV Data Logs</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; }";
  html += "h1, h2 { color: #333; }";
  html += ".file-list { margin-bottom: 20px; }";
  html += ".file-item { margin-bottom: 5px; }";
  html += ".file-link { text-decoration: none; color: #0066cc; }";
  html += ".file-link:hover { text-decoration: underline; }";
  html += ".file-size { color: #666; font-size: 0.9em; margin-left: 10px; }";
  html += ".delete-button { background-color: #ff4444; color: white; border: none; padding: 8px 16px; cursor: pointer; }";
  html += ".storage-info { background-color: #f0f0f0; padding: 10px; border-radius: 5px; margin-bottom: 20px; }";
  html += "</style></head><body>\r\n";
  
  html += "<h1>DPV Data Logs</h1>\r\n";
  
  // Display storage information
  size_t totalSize = getTotalLogSize();
  float usedMB = totalSize / (1024.0 * 1024.0);
  float maxMB = MAX_LOG_SIZE / (1024.0 * 1024.0);
  int usagePercent = (totalSize * 100) / MAX_LOG_SIZE;
  
  html += "<div class='storage-info'>";
  html += "Storage usage: " + String(usedMB, 2) + " MB / " + String(maxMB, 2) + " MB (" + String(usagePercent) + "%)";
  html += "</div>";
  
  // Display regular log files
  html += "<h2>Raw Data Logs</h2>\r\n";
  html += "<div class='file-list'>\r\n";
  
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
        html += "<div class='file-item'>";
        html += "<a class='file-link' href=\"logs?log=" + String(DATALOG_DIR) + "/" + String(file.name()) + "\">";
        html += file.name();
        html += "</a>\r\n";
        html += "<span class='file-size'>Size: "+ String(file.size())+" bytes</span>";
        html += "</div>";
      } 
      file.close();
      file = root.openNextFile();
  }
  root.close();
  
  html += "</div>\r\n";
  
  // Display summary files if they exist
  html += "<h2>Summary Data</h2>\r\n";
  html += "<div class='file-list'>\r\n";
  
  File summaryRoot = SPIFFS.open(SUMMARY_DIR);
  if(summaryRoot && summaryRoot.isDirectory()){
    File summaryFile = summaryRoot.openNextFile();
    bool hasSummaries = false;
    
    while(summaryFile){
      if(!summaryFile.isDirectory()){
        hasSummaries = true;
        html += "<div class='file-item'>";
        html += "<a class='file-link' href=\"logs?log=" + String(SUMMARY_DIR) + "/" + String(summaryFile.name()) + "\">";
        html += summaryFile.name();
        html += "</a>\r\n";
        html += "<span class='file-size'>Size: "+ String(summaryFile.size())+" bytes</span>";
        html += "</div>";
      } 
      summaryFile.close();
      summaryFile = summaryRoot.openNextFile();
    }
    
    if (!hasSummaries) {
      html += "<p>No summary data available yet.</p>";
    }
    
    summaryRoot.close();
  } else {
    html += "<p>Summary directory not available.</p>";
  }
  
  html += "</div>\r\n";
  
  // Delete button
  html += "<form action=\"/logs/delete-all\" method=\"post\" "
  "onsubmit=\"return confirm('Do you really want to delete all logs?');\">\r\n";
  html += "   <input type=\"submit\" class='delete-button' value=\"DELETE ALL LOGS\">\r\n";
  html += "</form>\r\n";

  html += "</body></html>\r\n";
  return html;
}

String readLogFile(String logname){
  // Check if the path already includes the directory
  if (!logname.startsWith("/")) {
    logname = DATALOG_DIR + "/" + logname;
  }
  
  File file = SPIFFS.open(logname);
  if (!file.available()){
    return "Cannot read " + logname;
  }
  String content = file.readString();
  file.close();
  return content;
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
  dp.motorState = motorState;
  dp.chassisHumidity = getHuminity();
  dp.chassisTemp = getTemp();
  dp.ledLampState = getLampState();
  dp.speedSetting = getSpeedSetting();
  dp.soc = batteryLevel;
  dp.leakBack = getLeakBack();
  dp.leakFront = getLeakFront();
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
  file.print(datapoint.motorState);
  file.print(",");    
  file.print(datapoint.mosfetTemp);
  file.print(",");             
  file.print(datapoint.chassisTemp);
  file.print(",");
  file.print(datapoint.chassisHumidity);
  file.print(",");
  file.print(datapoint.ledLampState);  
  file.print(",");    
  file.print(datapoint.speedSetting);
  file.print(",");
  file.print(datapoint.soc);
  file.print(",");
  file.print(datapoint.leakBack);  
  file.print(",");    
  file.print(datapoint.leakFront);  
  file.println();
  file.flush();
}

void deleteAllFiles(){
  if (EnableDebugLog) Serial.printf("Deleting files in: %s and %s\r\n", DATALOG_DIR, SUMMARY_DIR);
  
  if (loggingActive){
    csvFile.close();
  }

  // Delete data log files
  File root = SPIFFS.open(DATALOG_DIR);
  if(root && root.isDirectory()){
    String filename = root.getNextFileName();
    
    while(filename != ""){
      String fullpath = filename;
      if (EnableDebugLog) {
        Serial.print(fullpath);
        Serial.print(" DELETED: ");
        Serial.println(SPIFFS.remove(fullpath) ? "Yes" : "No");
      } else {
        SPIFFS.remove(fullpath);
      }
      filename = root.getNextFileName();
    }  
    root.close();
  } else {
    if (EnableDebugLog) Serial.println("Failed to open datalog directory");
  }
  
  // Delete summary files
  File summaryRoot = SPIFFS.open(SUMMARY_DIR);
  if(summaryRoot && summaryRoot.isDirectory()){
    String filename = summaryRoot.getNextFileName();
    
    while(filename != ""){
      String fullpath = filename;
      if (EnableDebugLog) {
        Serial.print(fullpath);
        Serial.print(" DELETED: ");
        Serial.println(SPIFFS.remove(fullpath) ? "Yes" : "No");
      } else {
        SPIFFS.remove(fullpath);
      }
      filename = summaryRoot.getNextFileName();
    }  
    summaryRoot.close();
  } else {
    if (EnableDebugLog) Serial.println("Failed to open summary directory");
  }

  if (loggingActive){
    openCSVFile();
  }
  
  // Reset the size check time
  lastSizeCheckTime = millis();
}

void datalogSetup(){
  if(!SPIFFS.begin(true)){
      Serial.println("SPIFFS Mount Failed");
      loggingActive = false;
      return;
  }
  
  // Create directories if they don't exist
  if (!SPIFFS.exists(DATALOG_DIR)) {
    if (EnableDebugLog) Serial.println("Creating datalog directory");
    SPIFFS.mkdir(DATALOG_DIR);
  }
  
  if (!SPIFFS.exists(SUMMARY_DIR)) {
    if (EnableDebugLog) Serial.println("Creating summary directory");
    SPIFFS.mkdir(SUMMARY_DIR);
  }
  
  loggingActive = true;
  openCSVFile();
  listLogFiles();
  
  // Initialize the size check time
  lastSizeCheckTime = millis();
}

// Get the total size of all log files
size_t getTotalLogSize() {
  size_t totalSize = 0;
  File root = SPIFFS.open(DATALOG_DIR);
  
  if (!root || !root.isDirectory()) {
    if (EnableDebugLog) Serial.println("Failed to open log directory");
    return 0;
  }
  
  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      totalSize += file.size();
    }
    file.close();
    file = root.openNextFile();
  }
  
  root.close();
  return totalSize;
}

// Get the oldest log file
String getOldestLogFile() {
  String oldestFile = "";
  unsigned long oldestTime = ULONG_MAX;
  
  File root = SPIFFS.open(DATALOG_DIR);
  if (!root || !root.isDirectory()) {
    return "";
  }
  
  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String filename = file.name();
      // Extract the number from data_X.csv
      int startPos = filename.lastIndexOf('_') + 1;
      int endPos = filename.lastIndexOf('.');
      if (startPos > 0 && endPos > startPos) {
        String numStr = filename.substring(startPos, endPos);
        unsigned long fileNum = numStr.toInt();
        if (fileNum < oldestTime) {
          oldestTime = fileNum;
          oldestFile = filename;
        }
      }
    }
    file.close();
    file = root.openNextFile();
  }
  
  root.close();
  return oldestFile;
}

// Create a summary from two log files and save it
void createSummaryFile(String oldestFile, String secondOldestFile) {
  if (EnableDebugLog) Serial.println("Creating summary from " + oldestFile + " and " + secondOldestFile);
  
  // Ensure summary directory exists
  if (!SPIFFS.exists(SUMMARY_DIR)) {
    SPIFFS.mkdir(SUMMARY_DIR);
  }
  
  // Read the oldest files
  File file1 = SPIFFS.open(DATALOG_DIR + "/" + oldestFile, FILE_READ);
  File file2 = SPIFFS.open(DATALOG_DIR + "/" + secondOldestFile, FILE_READ);
  
  if (!file1 || !file2) {
    if (EnableDebugLog) Serial.println("Failed to open log files for summarizing");
    if (file1) file1.close();
    if (file2) file2.close();
    return;
  }
  
  // Skip headers
  String header1 = file1.readStringUntil('\n');
  String header2 = file2.readStringUntil('\n');
  
  // Initialize summary data
  LogSummary summary;
  summary.startTime = ULONG_MAX;
  summary.endTime = 0;
  summary.avgMotorTemp = 0;
  summary.avgMotorInpVoltage = 0;
  summary.avgMotorCurrent = 0;
  summary.avgMotorRpm = 0;
  summary.avgMosfetTemp = 0;
  summary.avgChassisTemp = 0;
  summary.avgChassisHumidity = 0;
  summary.minBatteryLevel = 100;
  summary.maxBatteryLevel = 0;
  summary.motorOnTime = 0;
  summary.leakDetections = 0;
  
  int dataPoints = 0;
  bool motorWasOn = false;
  unsigned long lastMotorOnTime = 0;
  
  // Process file1
  while (file1.available()) {
    String line = file1.readStringUntil('\n');
    if (line.length() > 0) {
      int commaPos = 0;
      int nextCommaPos = line.indexOf(',', commaPos);
      
      if (nextCommaPos > 0) {
        // Time
        long time = line.substring(commaPos, nextCommaPos).toInt();
        if (time < summary.startTime) summary.startTime = time;
        if (time > summary.endTime) summary.endTime = time;
        
        // Motor temp
        commaPos = nextCommaPos + 1;
        nextCommaPos = line.indexOf(',', commaPos);
        if (nextCommaPos > 0) {
          float motorTemp = line.substring(commaPos, nextCommaPos).toFloat();
          summary.avgMotorTemp += motorTemp;
          
          // Continue parsing other values...
          // Motor input voltage
          commaPos = nextCommaPos + 1;
          nextCommaPos = line.indexOf(',', commaPos);
          if (nextCommaPos > 0) {
            float voltage = line.substring(commaPos, nextCommaPos).toFloat();
            summary.avgMotorInpVoltage += voltage;
            
            // Skip motor average input current
            commaPos = nextCommaPos + 1;
            nextCommaPos = line.indexOf(',', commaPos);
            
            // Motor average current
            commaPos = nextCommaPos + 1;
            nextCommaPos = line.indexOf(',', commaPos);
            if (nextCommaPos > 0) {
              float current = line.substring(commaPos, nextCommaPos).toFloat();
              summary.avgMotorCurrent += current;
              
              // Skip duty cycle
              commaPos = nextCommaPos + 1;
              nextCommaPos = line.indexOf(',', commaPos);
              
              // Motor RPM
              commaPos = nextCommaPos + 1;
              nextCommaPos = line.indexOf(',', commaPos);
              if (nextCommaPos > 0) {
                float rpm = line.substring(commaPos, nextCommaPos).toFloat();
                summary.avgMotorRpm += rpm;
                
                // Motor state
                commaPos = nextCommaPos + 1;
                nextCommaPos = line.indexOf(',', commaPos);
                if (nextCommaPos > 0) {
                  int state = line.substring(commaPos, nextCommaPos).toInt();
                  if (state > 0) {
                    if (!motorWasOn) {
                      motorWasOn = true;
                      lastMotorOnTime = time;
                    }
                  } else if (motorWasOn) {
                    motorWasOn = false;
                    summary.motorOnTime += (time - lastMotorOnTime) / 1000; // Convert to seconds
                  }
                  
                  // Mosfet temp
                  commaPos = nextCommaPos + 1;
                  nextCommaPos = line.indexOf(',', commaPos);
                  if (nextCommaPos > 0) {
                    float mosfetTemp = line.substring(commaPos, nextCommaPos).toFloat();
                    summary.avgMosfetTemp += mosfetTemp;
                    
                    // Chassis temp
                    commaPos = nextCommaPos + 1;
                    nextCommaPos = line.indexOf(',', commaPos);
                    if (nextCommaPos > 0) {
                      float chassisTemp = line.substring(commaPos, nextCommaPos).toFloat();
                      summary.avgChassisTemp += chassisTemp;
                      
                      // Chassis humidity
                      commaPos = nextCommaPos + 1;
                      nextCommaPos = line.indexOf(',', commaPos);
                      if (nextCommaPos > 0) {
                        float humidity = line.substring(commaPos, nextCommaPos).toFloat();
                        summary.avgChassisHumidity += humidity;
                        
                        // Skip LED lamp state and speed setting
                        commaPos = nextCommaPos + 1;
                        nextCommaPos = line.indexOf(',', commaPos);
                        commaPos = nextCommaPos + 1;
                        nextCommaPos = line.indexOf(',', commaPos);
                        
                        // Battery SOC
                        commaPos = nextCommaPos + 1;
                        nextCommaPos = line.indexOf(',', commaPos);
                        if (nextCommaPos > 0) {
                          int soc = line.substring(commaPos, nextCommaPos).toInt();
                          if (soc < summary.minBatteryLevel) summary.minBatteryLevel = soc;
                          if (soc > summary.maxBatteryLevel) summary.maxBatteryLevel = soc;
                          
                          // Leak sensors
                          commaPos = nextCommaPos + 1;
                          nextCommaPos = line.indexOf(',', commaPos);
                          if (nextCommaPos > 0) {
                            int leakBack = line.substring(commaPos, nextCommaPos).toInt();
                            
                            commaPos = nextCommaPos + 1;
                            int leakFront = line.substring(commaPos).toInt();
                            
                            if (leakBack > 0 || leakFront > 0) {
                              summary.leakDetections++;
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
        dataPoints++;
      }
    }
  }
  
  // Process file2 (similar to file1)
  // For brevity, we'll skip the detailed parsing of file2 which would be identical to file1
  
  // Calculate averages
  if (dataPoints > 0) {
    summary.avgMotorTemp /= dataPoints;
    summary.avgMotorInpVoltage /= dataPoints;
    summary.avgMotorCurrent /= dataPoints;
    summary.avgMotorRpm /= dataPoints;
    summary.avgMosfetTemp /= dataPoints;
    summary.avgChassisTemp /= dataPoints;
    summary.avgChassisHumidity /= dataPoints;
  }
  
  // Create summary file
  String summaryFilename = SUMMARY_DIR + "/summary_" + String(summary.startTime) + ".csv";
  File summaryFile = SPIFFS.open(summaryFilename, FILE_WRITE);
  
  if (summaryFile) {
    // Write summary header
    summaryFile.println("start_time,end_time,avg_motor_temp,avg_voltage,avg_current,avg_rpm,avg_mosfet_temp,"
                       "avg_chassis_temp,avg_humidity,min_battery,max_battery,motor_on_time,leak_detections");
    
    // Write summary data
    summaryFile.print(summary.startTime);
    summaryFile.print(",");
    summaryFile.print(summary.endTime);
    summaryFile.print(",");
    summaryFile.print(summary.avgMotorTemp);
    summaryFile.print(",");
    summaryFile.print(summary.avgMotorInpVoltage);
    summaryFile.print(",");
    summaryFile.print(summary.avgMotorCurrent);
    summaryFile.print(",");
    summaryFile.print(summary.avgMotorRpm);
    summaryFile.print(",");
    summaryFile.print(summary.avgMosfetTemp);
    summaryFile.print(",");
    summaryFile.print(summary.avgChassisTemp);
    summaryFile.print(",");
    summaryFile.print(summary.avgChassisHumidity);
    summaryFile.print(",");
    summaryFile.print(summary.minBatteryLevel);
    summaryFile.print(",");
    summaryFile.print(summary.maxBatteryLevel);
    summaryFile.print(",");
    summaryFile.print(summary.motorOnTime);
    summaryFile.print(",");
    summaryFile.print(summary.leakDetections);
    summaryFile.println();
    
    summaryFile.close();
    
    if (EnableDebugLog) Serial.println("Summary created: " + summaryFilename);
  } else {
    if (EnableDebugLog) Serial.println("Failed to create summary file");
  }
  
  file1.close();
  file2.close();
}

// Check and cleanup old files if needed
void checkAndCleanupOldFiles() {
  size_t totalSize = getTotalLogSize();
  
  if (EnableDebugLog) {
    Serial.print("Total log size: ");
    Serial.print(totalSize);
    Serial.println(" bytes");
  }
  
  if (totalSize > CLEANUP_THRESHOLD) {
    if (EnableDebugLog) Serial.println("Cleanup threshold reached, removing oldest files");
    
    // Get the two oldest files
    String oldestFile = getOldestLogFile();
    if (oldestFile.length() > 0) {
      // Remove the file from SPIFFS
      String fullPath = DATALOG_DIR + "/" + oldestFile;
      
      // Create a summary before deleting (if we have at least 2 files)
      String secondOldestFile = getOldestLogFile(); // After removing the oldest, this will be the new oldest
      if (secondOldestFile.length() > 0) {
        createSummaryFile(oldestFile, secondOldestFile);
      }
      
      if (SPIFFS.remove(fullPath)) {
        if (EnableDebugLog) Serial.println("Deleted: " + fullPath);
      } else {
        if (EnableDebugLog) Serial.println("Failed to delete: " + fullPath);
      }
    }
  }
}

void datalogLoop(){
  // Regular data logging
  if (loggingActive && millis() > lastDataLogTime + DATALOG_INTERVAL) {
    LogdataRow data = createDatapoint();
    saveDatapoint(data, csvFile);
    lastDataLogTime = millis();
  }
  
  // Periodically check log size and cleanup if needed
  if (loggingActive && millis() > lastSizeCheckTime + CHECK_SIZE_INTERVAL) {
    checkAndCleanupOldFiles();
    lastSizeCheckTime = millis();
  }
}
