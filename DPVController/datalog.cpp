#include "datalog.h"
#include "log.h"
#include <FS.h>
#include <SPIFFS.h>
#include "motor.h"
#include "main.h"
#include "battery.h"

/**
* Regularly saves data about the state of the vehicle to disc.
* Runs on a separate core to avoid affecting the main application.
*/

/*
* CONSTANTS
*/ 

const String HEADER = "timestamp,motor_temp,mosfet_temp,battery_voltage,input_current,motor_current,rpm,duty_cycle,temperature,humidity,battery_level,leak_sensor,led_state,total_uptime";
const String DATALOG_DIR = "/datalog";
const unsigned long DATALOG_INTERVAL = 1000; // Wie oft ein Datenpunkt gespeichert wird (ms)
const int MAX_LOG_FILES = 10; // Maximale Anzahl an Log-Dateien
const double MAX_SPEED_RPM = 15800; // Maximum speed in rpm. Speed of 100%, kopiert aus motor.cpp

/*
* GLOBAL VARIABLES
*/ 
TaskHandle_t dataloggerTaskHandle = NULL;
File csvFile;
unsigned long lastDataLogTime = 0;

// Multi-level data storage
LogdataRow recentData[MAX_RECENT_POINTS];        // 1s resolution - last 30 min
LogdataRow hourlyData[MAX_HOURLY_POINTS];        // 1min resolution - last 2 hours
LogdataRow historicalData[MAX_HISTORICAL_POINTS]; // 5min resolution - last 6 hours

int recentIndex = 0;
int hourlyIndex = 0;
int historicalIndex = 0;
int totalRecentPoints = 0;
int totalHourlyPoints = 0;
int totalHistoricalPoints = 0;

// Persistence and compression tracking
unsigned long lastHourlySave = 0;
unsigned long lastHistoricalSave = 0;
const unsigned long HOURLY_COMPRESSION_INTERVAL = 60000;    // Compress every minute
const unsigned long HISTORICAL_COMPRESSION_INTERVAL = 300000; // Compress every 5 minutes
const unsigned long PERSISTENCE_SAVE_INTERVAL = 300000;     // Save to SPIFFS every 5 minutes

bool isDataloggerRunning = false;
unsigned long totalUptimeSeconds = 0;
unsigned long lastUptimeSave = 0;
const unsigned long UPTIME_SAVE_INTERVAL = 60000; // Save every minute

/**
 * Lädt die gespeicherte Total-Uptime aus SPIFFS
 */
void loadTotalUptime() {
  if (SPIFFS.exists("/total_uptime.txt")) {
    File file = SPIFFS.open("/total_uptime.txt", "r");
    if (file) {
      String uptimeStr = file.readString();
      totalUptimeSeconds = uptimeStr.toInt();
      file.close();
      String loadMsg = "Total uptime geladen: " + String(totalUptimeSeconds) + " Sekunden";
      log(loadMsg.c_str());
    }
  } else {
    totalUptimeSeconds = 0;
    log("Keine gespeicherte Total-Uptime gefunden, starte bei 0");
  }
}

/**
 * Speichert die aktuelle Total-Uptime in SPIFFS
 */
void saveTotalUptime() {
  File file = SPIFFS.open("/total_uptime.txt", "w");
  if (file) {
    file.println(totalUptimeSeconds);
    file.close();
  }
}

/**
 * Gibt die Total-Uptime in Sekunden zurück
 */
unsigned long getTotalUptime() {
  return totalUptimeSeconds + (millis() / 1000);
}

/**
 * Öffnet eine neue CSV-Datei zum Schreiben
 */
void openCSVFile() {
  // Kurze Pause vor Dateizugriff
  vTaskDelay(10 / portTICK_PERIOD_MS);
  
  String filename;
  for(int i = 0; i < 100; i++) { // Limit to prevent infinite loop
    filename = DATALOG_DIR + "/data_" + String(i) + ".csv";
    if (!SPIFFS.exists(filename)) break;
    // Kurze Pause während der Dateisuche
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
  
  String openMsg = "Attempting to open CSV file: " + filename;
  log(openMsg.c_str());
  
  csvFile = SPIFFS.open(filename, FILE_WRITE);
  
  if (csvFile) {
    String successMsg = "CSV file opened successfully: " + filename;
    log(successMsg.c_str());
    csvFile.println(HEADER);
    csvFile.flush();
    log("CSV header written and flushed");
  } else {
    String errorMsg = "WARNING: Failed to open CSV file: " + filename + " - continuing without CSV logging";
    log(errorMsg.c_str());
  }
  
  // Kurze Pause nach Dateischreiben
  vTaskDelay(10 / portTICK_PERIOD_MS);
}

/**
 * Listen alle Log-Dateien auf
 */
void listLogFiles() {
  String logMessage = "Auflisten des Verzeichnisses: " + String(DATALOG_DIR);
  log(logMessage.c_str());

  File root = SPIFFS.open(DATALOG_DIR);
  if(!root) {
    log("- Konnte Verzeichnis nicht öffnen");
    return;
  }
  if(!root.isDirectory()) {
    log("- Ist kein Verzeichnis");
    return;
  }

  File file = root.openNextFile();
  while(file) {
    if(file.isDirectory()) {
      String dirMessage = "  DIR : " + String(file.name());
      log(dirMessage.c_str());
    } else {
      String fileMessage = "  FILE: " + String(file.name()) + " SIZE: " + String(file.size());
      log(fileMessage.c_str());
    }
    file = root.openNextFile();
  }
}

/**
 * Zählt die Anzahl der Log-Dateien
 */
int countLogFiles() {
  int count = 0;
  File root = SPIFFS.open(DATALOG_DIR);
  if(!root || !root.isDirectory()) return 0;

  File file = root.openNextFile();
  while(file) {
    if(!file.isDirectory()) {
      count++;
    }
    file = root.openNextFile();
  }
  return count;
}

/**
 * Löscht die älteste Log-Datei
 */
void deleteOldestLogFile() {
  String oldestFile = "";
  int oldestIndex = 99999;
  
  File root = SPIFFS.open(DATALOG_DIR);
  if(!root || !root.isDirectory()) return;
  
  File file = root.openNextFile();
  while(file) {
    if(!file.isDirectory()) {
      String name = String(file.name());
      if(name.startsWith(DATALOG_DIR + "/data_") && name.endsWith(".csv")) {
        // Extrahiere Index aus Dateinamen
        int idx = name.substring(DATALOG_DIR.length() + 6, name.length() - 4).toInt();
        if(idx < oldestIndex) {
          oldestIndex = idx;
          oldestFile = name;
        }
      }
    }
    file = root.openNextFile();
  }
  
  if(oldestFile != "") {
    if(SPIFFS.remove(oldestFile)) {
      String deleteMessage = "Älteste Datei gelöscht: " + oldestFile;
      log(deleteMessage.c_str());
    } else {
      String errorMessage = "Fehler beim Löschen der Datei: " + oldestFile;
      log(errorMessage.c_str());
    }
  }
}

/**
 * Gibt den Inhalt einer Log-Datei zurück
 */
String getLogFileContent(String filename) {
  if(!SPIFFS.exists(filename)) {
    return "File not found";
  }
  
  File file = SPIFFS.open(filename, FILE_READ);
  if(!file) {
    return "Failed to open file";
  }
  
  String content = "";
  while(file.available()) {
    content += file.readString();
  }
  file.close();
  return content;
}

/**
 * Gibt den Pfad zur neuesten Log-Datei zurück
 */
String getNewestLogFile() {
  String newestFile = "";
  int newestIndex = -1;
  
  File root = SPIFFS.open(DATALOG_DIR);
  if(!root || !root.isDirectory()) return "";
  
  File file = root.openNextFile();
  while(file) {
    if(!file.isDirectory()) {
      String name = String(file.name());
      if(name.startsWith(DATALOG_DIR + "/data_") && name.endsWith(".csv")) {
        // Extrahiere Index aus Dateinamen
        int idx = name.substring(DATALOG_DIR.length() + 6, name.length() - 4).toInt();
        if(idx > newestIndex) {
          newestIndex = idx;
          newestFile = name;
        }
      }
    }
    file = root.openNextFile();
  }
  
  return newestFile;
}

/**
 * Erstellt einen neuen Datenpunkt mit aktuellen Werten
 */
LogdataRow createDatapoint() {
  LogdataRow dp;
  dp.timestamp = millis();
  
  log("Creating datapoint - step 1: timestamp set");
  
  // Simplified data creation with step-by-step debugging
  // Motortemperatur und MOSFET-Temperatur
  if (HAS_MOTOR) {
    dp.tempMotor = getVescUart().data.tempMotor;
    dp.tempMosfet = getVescUart().data.tempMosfet;
  } else {
    dp.tempMotor = 20.0 + (loopCount % 10);
    dp.tempMosfet = 25.0 + (loopCount % 15);
  }
  log("Creating datapoint - step 2: motor temps set");
  
  // Batteriespannung
  dp.batteryVoltage = getBatteryVoltage();
  log("Creating datapoint - step 3: battery voltage set");
  
  // Weitere VESC-Daten
  if (HAS_MOTOR) {
    dp.current = getVescUart().data.avgInputCurrent;
    dp.avgMotorCurrent = getVescUart().data.avgMotorCurrent;
    dp.rpm = getVescUart().data.rpm;
    dp.dutyCycle = getVescUart().data.dutyCycleNow;
  } else {
    // Simulierte Werte, falls kein Motor verfügbar
    dp.current = 5.0 + (loopCount % 20);
    dp.avgMotorCurrent = 3.0 + (loopCount % 15);
    dp.rpm = 1000 + (loopCount % 1000);
    dp.dutyCycle = 25.0 + (loopCount % 50);
  }
  log("Creating datapoint - step 4: VESC data set");
  
  // Umgebungstemperatur und Luftfeuchtigkeit von DHT-Sensor
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  
  // Handle NaN values from DHT sensor
  if (isnan(data.temperature)) {
    dp.temperature = 0.0; // Default fallback value
  } else {
    dp.temperature = data.temperature;
  }
  
  if (isnan(data.humidity)) {
    dp.humidity = 0.0; // Default fallback value
  } else {
    dp.humidity = data.humidity;
  }
  log("Creating datapoint - step 5: DHT data set");
  
  // Battery Level, Leak Sensor, LED State, Total Uptime
  dp.batteryLevel = batteryLevel;
  dp.leakSensorState = leakSensorState;
  dp.ledState = 0; // TODO: Get actual LED state from LED bar
  dp.totalUptime = getTotalUptime();
  
  log("Creating datapoint - step 6: additional data set");
  
  log("Datapoint creation completed successfully");
  return dp;
}

/**
 * Speichert einen Datenpunkt in die CSV-Datei
 */
void saveDatapoint(LogdataRow datapoint, File &file) {
  // Kurze Pause vor Dateischreiben
  vTaskDelay(5 / portTICK_PERIOD_MS);
  
  file.print(datapoint.timestamp);
  file.print(",");
  file.print(datapoint.tempMotor);
  file.print(",");
  file.print(datapoint.tempMosfet);
  file.print(",");
  file.print(datapoint.batteryVoltage);
  file.print(",");
  file.print(datapoint.current);
  file.print(",");
  file.print(datapoint.avgMotorCurrent);
  file.print(",");
  file.print(datapoint.rpm);
  file.print(",");
  file.print(datapoint.dutyCycle);
  file.print(",");
  file.print(datapoint.temperature);
  file.print(",");
  file.print(datapoint.humidity);
  file.print(",");
  file.print(datapoint.batteryLevel);
  file.print(",");
  file.print(datapoint.leakSensorState);
  file.print(",");
  file.print(datapoint.ledState);
  file.print(",");
  file.print(datapoint.totalUptime);
  file.println();
  file.flush();
  
  // Kurze Pause nach Dateischreiben
  vTaskDelay(5 / portTICK_PERIOD_MS);
}

/**
 * Fügt einen Datenpunkt zum Recent-Buffer hinzu (1s Auflösung)
 */
void addToRecentData(LogdataRow datapoint) {
  String beforeMsg = "Adding datapoint to recent buffer - Index: " + String(recentIndex) + ", Total: " + String(totalRecentPoints);
  log(beforeMsg.c_str());
  
  recentData[recentIndex] = datapoint;
  recentIndex = (recentIndex + 1) % MAX_RECENT_POINTS;
  if (totalRecentPoints < MAX_RECENT_POINTS) {
    totalRecentPoints++;
  }
  
  String afterMsg = "Datapoint added to recent - New Index: " + String(recentIndex) + ", New Total: " + String(totalRecentPoints);
  log(afterMsg.c_str());
}

/**
 * Komprimiert Recent-Daten zu Hourly-Daten (1 Minute Durchschnitt)
 */
void compressToHourlyData() {
  if (totalRecentPoints < 60) return; // Need at least 1 minute of data
  
  // Calculate average of last 60 data points
  LogdataRow avgPoint = {0};
  int count = 0;
  
  for (int i = 0; i < 60 && i < totalRecentPoints; i++) {
    int idx = (recentIndex - 1 - i + MAX_RECENT_POINTS) % MAX_RECENT_POINTS;
    LogdataRow& point = recentData[idx];
    
    if (count == 0) {
      avgPoint = point; // Initialize with first point
    } else {
      avgPoint.tempMotor = (avgPoint.tempMotor * count + point.tempMotor) / (count + 1);
      avgPoint.tempMosfet = (avgPoint.tempMosfet * count + point.tempMosfet) / (count + 1);
      avgPoint.batteryVoltage = (avgPoint.batteryVoltage * count + point.batteryVoltage) / (count + 1);
      avgPoint.current = (avgPoint.current * count + point.current) / (count + 1);
      avgPoint.avgMotorCurrent = (avgPoint.avgMotorCurrent * count + point.avgMotorCurrent) / (count + 1);
      avgPoint.rpm = (avgPoint.rpm * count + point.rpm) / (count + 1);
      avgPoint.dutyCycle = (avgPoint.dutyCycle * count + point.dutyCycle) / (count + 1);
      avgPoint.temperature = (avgPoint.temperature * count + point.temperature) / (count + 1);
      avgPoint.humidity = (avgPoint.humidity * count + point.humidity) / (count + 1);
      avgPoint.batteryLevel = (avgPoint.batteryLevel * count + point.batteryLevel) / (count + 1);
      // Keep latest values for discrete data
      avgPoint.leakSensorState = point.leakSensorState;
      avgPoint.ledState = point.ledState;
      avgPoint.totalUptime = point.totalUptime;
    }
    count++;
  }
  
  // Use timestamp of most recent point
  avgPoint.timestamp = recentData[(recentIndex - 1 + MAX_RECENT_POINTS) % MAX_RECENT_POINTS].timestamp;
  
  // Add to hourly buffer
  hourlyData[hourlyIndex] = avgPoint;
  hourlyIndex = (hourlyIndex + 1) % MAX_HOURLY_POINTS;
  if (totalHourlyPoints < MAX_HOURLY_POINTS) {
    totalHourlyPoints++;
  }
  
  String compressMsg = "Compressed to hourly data - Index: " + String(hourlyIndex) + ", Total: " + String(totalHourlyPoints);
  log(compressMsg.c_str());
}

/**
 * Komprimiert Hourly-Daten zu Historical-Daten (5 Minuten Durchschnitt)
 */
void compressToHistoricalData() {
  if (totalHourlyPoints < 5) return; // Need at least 5 minutes of data
  
  // Calculate average of last 5 hourly points
  LogdataRow avgPoint = {0};
  int count = 0;
  
  for (int i = 0; i < 5 && i < totalHourlyPoints; i++) {
    int idx = (hourlyIndex - 1 - i + MAX_HOURLY_POINTS) % MAX_HOURLY_POINTS;
    LogdataRow& point = hourlyData[idx];
    
    if (count == 0) {
      avgPoint = point;
    } else {
      avgPoint.tempMotor = (avgPoint.tempMotor * count + point.tempMotor) / (count + 1);
      avgPoint.tempMosfet = (avgPoint.tempMosfet * count + point.tempMosfet) / (count + 1);
      avgPoint.batteryVoltage = (avgPoint.batteryVoltage * count + point.batteryVoltage) / (count + 1);
      avgPoint.current = (avgPoint.current * count + point.current) / (count + 1);
      avgPoint.avgMotorCurrent = (avgPoint.avgMotorCurrent * count + point.avgMotorCurrent) / (count + 1);
      avgPoint.rpm = (avgPoint.rpm * count + point.rpm) / (count + 1);
      avgPoint.dutyCycle = (avgPoint.dutyCycle * count + point.dutyCycle) / (count + 1);
      avgPoint.temperature = (avgPoint.temperature * count + point.temperature) / (count + 1);
      avgPoint.humidity = (avgPoint.humidity * count + point.humidity) / (count + 1);
      avgPoint.batteryLevel = (avgPoint.batteryLevel * count + point.batteryLevel) / (count + 1);
      avgPoint.leakSensorState = point.leakSensorState;
      avgPoint.ledState = point.ledState;
      avgPoint.totalUptime = point.totalUptime;
    }
    count++;
  }
  
  avgPoint.timestamp = hourlyData[(hourlyIndex - 1 + MAX_HOURLY_POINTS) % MAX_HOURLY_POINTS].timestamp;
  
  // Add to historical buffer
  historicalData[historicalIndex] = avgPoint;
  historicalIndex = (historicalIndex + 1) % MAX_HISTORICAL_POINTS;
  if (totalHistoricalPoints < MAX_HISTORICAL_POINTS) {
    totalHistoricalPoints++;
  }
  
  String compressMsg = "Compressed to historical data - Index: " + String(historicalIndex) + ", Total: " + String(totalHistoricalPoints);
  log(compressMsg.c_str());
}

/**
 * Speichert komprimierte Daten in SPIFFS für Persistenz
 */
void saveCompressedData() {
  log("Saving compressed data to SPIFFS...");
  
  // Save recent data (last 100 points to survive reboot)
  File recentFile = SPIFFS.open("/recent_data.bin", "w");
  if (recentFile) {
    int pointsToSave = totalRecentPoints > 100 ? 100 : totalRecentPoints;
    recentFile.write((uint8_t*)&pointsToSave, sizeof(int));
    recentFile.write((uint8_t*)&recentIndex, sizeof(int));
    
    // Save last 100 points in correct order
    for (int i = 0; i < pointsToSave; i++) {
      int idx = (recentIndex - pointsToSave + i + MAX_RECENT_POINTS) % MAX_RECENT_POINTS;
      recentFile.write((uint8_t*)&recentData[idx], sizeof(LogdataRow));
    }
    recentFile.close();
    log("Recent data saved to SPIFFS");
  } else {
    log("Failed to save recent data");
  }
  
  // Save hourly data
  File hourlyFile = SPIFFS.open("/hourly_data.bin", "w");
  if (hourlyFile) {
    hourlyFile.write((uint8_t*)&totalHourlyPoints, sizeof(int));
    hourlyFile.write((uint8_t*)&hourlyIndex, sizeof(int));
    hourlyFile.write((uint8_t*)hourlyData, sizeof(LogdataRow) * MAX_HOURLY_POINTS);
    hourlyFile.close();
    log("Hourly data saved to SPIFFS");
  } else {
    log("Failed to save hourly data");
  }
  
  // Save historical data
  File historicalFile = SPIFFS.open("/historical_data.bin", "w");
  if (historicalFile) {
    historicalFile.write((uint8_t*)&totalHistoricalPoints, sizeof(int));
    historicalFile.write((uint8_t*)&historicalIndex, sizeof(int));
    historicalFile.write((uint8_t*)historicalData, sizeof(LogdataRow) * MAX_HISTORICAL_POINTS);
    historicalFile.close();
    log("Historical data saved to SPIFFS");
  } else {
    log("Failed to save historical data");
  }
}

/**
 * Lädt komprimierte Daten aus SPIFFS nach einem Neustart
 */
void loadCompressedData() {
  log("Loading compressed data from SPIFFS...");
  
  // Initialize all values to safe defaults first
  totalRecentPoints = 0;
  recentIndex = 0;
  totalHourlyPoints = 0;
  hourlyIndex = 0;
  totalHistoricalPoints = 0;
  historicalIndex = 0;
  
  // Load recent data
  if (SPIFFS.exists("/recent_data.bin")) {
    File recentFile = SPIFFS.open("/recent_data.bin", "r");
    if (recentFile) {
      int savedPoints = 0;
      int savedIndex = 0;
      
      size_t bytesRead = recentFile.read((uint8_t*)&savedPoints, sizeof(int));
      if (bytesRead == sizeof(int) && savedPoints > 0 && savedPoints <= 100) {
        recentFile.read((uint8_t*)&savedIndex, sizeof(int));
        
        // Load points in correct order
        for (int i = 0; i < savedPoints; i++) {
          recentFile.read((uint8_t*)&recentData[i], sizeof(LogdataRow));
        }
        
        totalRecentPoints = savedPoints;
        recentIndex = savedPoints % MAX_RECENT_POINTS;
        
        String recentMsg = "Loaded recent data - Total: " + String(totalRecentPoints) + ", Index: " + String(recentIndex);
        log(recentMsg.c_str());
      } else {
        log("Invalid recent data, resetting");
      }
      recentFile.close();
    } else {
      log("Failed to open recent data file");
    }
  } else {
    log("No recent data file found");
  }
  
  // Load hourly data
  if (SPIFFS.exists("/hourly_data.bin")) {
    File hourlyFile = SPIFFS.open("/hourly_data.bin", "r");
    if (hourlyFile) {
      size_t bytesRead = hourlyFile.read((uint8_t*)&totalHourlyPoints, sizeof(int));
      if (bytesRead == sizeof(int)) {
        hourlyFile.read((uint8_t*)&hourlyIndex, sizeof(int));
        hourlyFile.read((uint8_t*)hourlyData, sizeof(LogdataRow) * MAX_HOURLY_POINTS);
        
        // Validate loaded data
        if (totalHourlyPoints < 0 || totalHourlyPoints > MAX_HOURLY_POINTS) {
          log("Invalid hourly data, resetting");
          totalHourlyPoints = 0;
          hourlyIndex = 0;
        } else {
          String hourlyMsg = "Loaded hourly data - Total: " + String(totalHourlyPoints) + ", Index: " + String(hourlyIndex);
          log(hourlyMsg.c_str());
        }
      }
      hourlyFile.close();
    } else {
      log("Failed to open hourly data file");
    }
  } else {
    log("No hourly data file found");
  }
  
  // Load historical data
  if (SPIFFS.exists("/historical_data.bin")) {
    File historicalFile = SPIFFS.open("/historical_data.bin", "r");
    if (historicalFile) {
      size_t bytesRead = historicalFile.read((uint8_t*)&totalHistoricalPoints, sizeof(int));
      if (bytesRead == sizeof(int)) {
        historicalFile.read((uint8_t*)&historicalIndex, sizeof(int));
        historicalFile.read((uint8_t*)historicalData, sizeof(LogdataRow) * MAX_HISTORICAL_POINTS);
        
        // Validate loaded data
        if (totalHistoricalPoints < 0 || totalHistoricalPoints > MAX_HISTORICAL_POINTS) {
          log("Invalid historical data, resetting");
          totalHistoricalPoints = 0;
          historicalIndex = 0;
        } else {
          String historicalMsg = "Loaded historical data - Total: " + String(totalHistoricalPoints) + ", Index: " + String(historicalIndex);
          log(historicalMsg.c_str());
        }
      }
      historicalFile.close();
    } else {
      log("Failed to open historical data file");
    }
  } else {
    log("No historical data file found");
  }
  
  log("Data loading completed successfully");
}

/**
 * Gibt die letzten n Datenpunkte zurück (mit automatischer Zeitbereich-Auswahl)
 */
LogdataRow* getLatestDataPoints(int count, String timeRange) {
  if (timeRange == "recent") {
    return getRecentData(count);
  } else if (timeRange == "hourly") {
    return getHourlyData(count);
  } else if (timeRange == "historical") {
    return getHistoricalData(count);
  } else {
    return getRecentData(count);
  }
}

/**
 * Gibt Recent-Daten zurück (1s Auflösung)
 */
LogdataRow* getRecentData(int count) {
  String requestMsg = "getRecentData called - Requested: " + String(count) + ", Available: " + String(totalRecentPoints);
  log(requestMsg.c_str());
  
  if (count > totalRecentPoints) count = totalRecentPoints;
  if (count <= 0) {
    log("No recent data points available, returning NULL");
    return NULL;
  }
  
  static LogdataRow result[MAX_RECENT_POINTS];
  
  int start = (recentIndex - count + MAX_RECENT_POINTS) % MAX_RECENT_POINTS;
  String startMsg = "Reading from recent buffer - Start index: " + String(start) + ", Count: " + String(count);
  log(startMsg.c_str());
  
  for (int i = 0; i < count; i++) {
    result[i] = recentData[(start + i) % MAX_RECENT_POINTS];
  }
  
  if (count > 0) {
    String firstMsg = "First recent datapoint - Timestamp: " + String(result[0].timestamp) + ", Battery: " + String(result[0].batteryVoltage);
    log(firstMsg.c_str());
    
    if (count > 1) {
      String lastMsg = "Last recent datapoint - Timestamp: " + String(result[count-1].timestamp) + ", Battery: " + String(result[count-1].batteryVoltage);
      log(lastMsg.c_str());
    }
  }
  
  return result;
}

/**
 * Gibt Hourly-Daten zurück (1min Auflösung)
 */
LogdataRow* getHourlyData(int count) {
  if (count > totalHourlyPoints) count = totalHourlyPoints;
  if (count <= 0) return NULL;
  
  static LogdataRow result[MAX_HOURLY_POINTS];
  
  int start = (hourlyIndex - count + MAX_HOURLY_POINTS) % MAX_HOURLY_POINTS;
  for (int i = 0; i < count; i++) {
    result[i] = hourlyData[(start + i) % MAX_HOURLY_POINTS];
  }
  
  String hourlyMsg = "Returning " + String(count) + " hourly data points";
  log(hourlyMsg.c_str());
  
  return result;
}

/**
 * Gibt Historical-Daten zurück (5min Auflösung)
 */
LogdataRow* getHistoricalData(int count) {
  if (count > totalHistoricalPoints) count = totalHistoricalPoints;
  if (count <= 0) return NULL;
  
  static LogdataRow result[MAX_HISTORICAL_POINTS];
  
  int start = (historicalIndex - count + MAX_HISTORICAL_POINTS) % MAX_HISTORICAL_POINTS;
  for (int i = 0; i < count; i++) {
    result[i] = historicalData[(start + i) % MAX_HISTORICAL_POINTS];
  }
  
  String historicalMsg = "Returning " + String(count) + " historical data points";
  log(historicalMsg.c_str());
  
  return result;
}

/**
 * Gibt die Anzahl verfügbarer Datenpunkte für einen Zeitbereich zurück
 */
int getTotalDataPoints(String timeRange) {
  if (timeRange == "recent") {
    return totalRecentPoints;
  } else if (timeRange == "hourly") {
    return totalHourlyPoints;
  } else if (timeRange == "historical") {
    return totalHistoricalPoints;
  } else {
    return totalRecentPoints;
  }
}

/**
 * Der Haupttask für den Datalogger, läuft auf Core 1
 */
void dataloggerTask(void *pvParameters) {
  log("=== DATALOGGER TASK STARTED ===");
  
  // Set running flag immediately
  isDataloggerRunning = true;
  log("isDataloggerRunning set to true");
  
  // Short delay
  vTaskDelay(100 / portTICK_PERIOD_MS);
  log("Initial delay completed");
  
  // Load persisted data from SPIFFS
  log("Loading persisted data...");
  loadTotalUptime();
  loadCompressedData();
  log("Persisted data loaded");
  
  // Create simple test datapoint immediately
  log("Creating simple test datapoint...");
  
  LogdataRow testData;
  testData.timestamp = millis();
  testData.tempMotor = 25.0;
  testData.tempMosfet = 30.0;
  testData.batteryVoltage = 12.5;
  testData.current = 1.0;
  testData.avgMotorCurrent = 0.8;
  testData.rpm = 500.0;
  testData.dutyCycle = 10.0;
  testData.temperature = 22.0;
  testData.humidity = 45.0;
  testData.batteryLevel = 75;
  testData.leakSensorState = 0;
  testData.ledState = 0;
  testData.totalUptime = 123; // Simple fixed value
  
  log("Test datapoint struct filled");
  
  // Add to buffer
  recentData[0] = testData;
  recentIndex = 1;
  totalRecentPoints = 1;
  
  log("Test datapoint added to buffer");
  
  String statusMsg = "Buffer status - Index: " + String(recentIndex) + ", Total: " + String(totalRecentPoints);
  log(statusMsg.c_str());
  
  // Save initial data immediately
  log("Saving initial data to SPIFFS...");
  saveTotalUptime();
  saveCompressedData();
  log("Initial data saved");

  // Main loop - create new datapoints every second
  log("Entering main loop...");
  
  unsigned long lastDataLogTime = millis();
  unsigned long lastPersistenceTime = millis();
  unsigned long lastHourlySave = millis();
  unsigned long lastHistoricalSave = millis();
  
  while (true) {
    unsigned long currentTime = millis();
    
    // Debug every 10 seconds to show we're alive
    static unsigned long lastDebugTime = 0;
    if (currentTime - lastDebugTime >= 10000) {
      String aliveMsg = "Datalogger task alive - Total points: " + String(totalRecentPoints);
      log(aliveMsg.c_str());
      lastDebugTime = currentTime;
    }
    
    // Create new datapoint every second
    if (currentTime - lastDataLogTime >= DATALOG_INTERVAL) {
      log("Creating new datapoint...");
      
      // Create datapoint with real sensor values
      LogdataRow newData;
      newData.timestamp = currentTime;
      
      // Real motor data from VESC
      if (HAS_MOTOR) {
        newData.tempMotor = getVescUart().data.tempMotor;
        newData.tempMosfet = getVescUart().data.tempMosfet;
        newData.current = getVescUart().data.avgInputCurrent;
        newData.avgMotorCurrent = getVescUart().data.avgMotorCurrent;
        newData.rpm = getVescUart().data.rpm;
        newData.dutyCycle = getVescUart().data.dutyCycleNow;
      } else {
        // Fallback values if no motor
        newData.tempMotor = 25.0;
        newData.tempMosfet = 30.0;
        newData.current = 1.0;
        newData.avgMotorCurrent = 0.8;
        newData.rpm = 0.0;
        newData.dutyCycle = 0.0;
      }
      
      // Real sensor values
      newData.batteryVoltage = getBatteryVoltage();
      
      // DHT sensor data with fallback
      TempAndHumidity dhtData = dhtSensor.getTempAndHumidity();
      newData.temperature = isnan(dhtData.temperature) ? 0.0 : dhtData.temperature;
      newData.humidity = isnan(dhtData.humidity) ? 0.0 : dhtData.humidity;
      
      // Real system status
      newData.batteryLevel = batteryLevel;
      newData.leakSensorState = leakSensorState;
      newData.ledState = 0; // TODO: Get real LED state
      newData.totalUptime = getTotalUptime();
      
      // Add to buffer
      recentData[recentIndex] = newData;
      recentIndex = (recentIndex + 1) % MAX_RECENT_POINTS;
      if (totalRecentPoints < MAX_RECENT_POINTS) {
        totalRecentPoints++;
      }
      
      lastDataLogTime = currentTime;
      
      String newPointMsg = "New datapoint added - Index: " + String(recentIndex) + ", Total: " + String(totalRecentPoints);
      log(newPointMsg.c_str());
      
      // Save data every 5 datapoints for extra safety
      if (totalRecentPoints % 5 == 0) {
        log("Saving data after 5 datapoints...");
        saveTotalUptime();
        saveCompressedData();
        log("Data saved after datapoint milestone");
      }
      
      // Compress data periodically
      if (currentTime - lastHourlySave >= HOURLY_COMPRESSION_INTERVAL) {
        compressToHourlyData();
        lastHourlySave = currentTime;
      }
      
      if (currentTime - lastHistoricalSave >= HISTORICAL_COMPRESSION_INTERVAL) {
        compressToHistoricalData();
        lastHistoricalSave = currentTime;
      }
    }
    
    // Save data to SPIFFS every 10 seconds for persistence
    if (currentTime - lastPersistenceTime >= 10000) { // 10 seconds
      log("Saving data for persistence...");
      saveTotalUptime();
      saveCompressedData();
      lastPersistenceTime = currentTime;
      log("Data saved to SPIFFS");
    }
    
    // Keep task alive
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

/**
 * Setup-Funktion für den Datalogger
 * Startet den Datalogger-Task auf Core 1
 */
void datalogSetup() {
  log("=== DATALOG SETUP START ===");
  
  // Initialize all buffer variables to safe defaults
  recentIndex = 0;
  hourlyIndex = 0;
  historicalIndex = 0;
  totalRecentPoints = 0;
  totalHourlyPoints = 0;
  totalHistoricalPoints = 0;
  isDataloggerRunning = false;
  
  log("Buffer variables initialized");
  
  // Erstelle Task auf Core 1 (nicht Core 0, da dort der Webserver läuft)
  BaseType_t taskResult = xTaskCreatePinnedToCore(
    dataloggerTask,        // Task-Funktion
    "DataloggerTask",      // Task-Name
    16000,                 // Stack-Größe (Bytes) - erhöht wegen Stack Overflow
    NULL,                  // Task-Parameter
    1,                     // Task-Priorität (1 ist niedrig)
    &dataloggerTaskHandle, // Task-Handle
    1                      // Core-ID (1)
  );
  
  if (taskResult == pdPASS) {
    log("Datalogger-Task SUCCESSFULLY created on Core 1");
  } else {
    log("ERROR: Failed to create Datalogger-Task!");
  }
  
  log("=== DATALOG SETUP END ===");
}

/**
 * Loop-Funktion für den Datalogger
 * Diese wird aus der Hauptschleife aufgerufen, tut aber nichts,
 * da der eigentliche Datalogger auf einem anderen Kern läuft
 */
void datalogLoop() {
  // Der eigentliche Datalogger läuft in einem separaten Task,
  // hier ist nichts zu tun.
}
