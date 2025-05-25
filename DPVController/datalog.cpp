#include "datalog.h"
#include "log.h"
#include <FS.h>
#include <LittleFS.h>
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

// Simple append-only data storage
LogdataRow recentData[MAX_RECENT_POINTS];        // RAM buffer for live display

int recentIndex = 0;
int totalRecentPoints = 0;

// Trip log file handle
File tripLogFile;

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
  if (LittleFS.exists("/total_uptime.txt")) {
    File file = LittleFS.open("/total_uptime.txt", "r");
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
  File file = LittleFS.open("/total_uptime.txt", "w");
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
    if (!LittleFS.exists(filename)) break;
    // Kurze Pause während der Dateisuche
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
  
  String openMsg = "Attempting to open CSV file: " + filename;
  log(openMsg.c_str());
  
  csvFile = LittleFS.open(filename, FILE_WRITE);
  
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

    File root = LittleFS.open(DATALOG_DIR);
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
  File root = LittleFS.open(DATALOG_DIR);
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
  
  File root = LittleFS.open(DATALOG_DIR);
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
    if(LittleFS.remove(oldestFile)) {
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
  if(!LittleFS.exists(filename)) {
    return "File not found";
  }
  
  File file = LittleFS.open(filename, FILE_READ);
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
  
  File root = LittleFS.open(DATALOG_DIR);
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
 * Initialize trip log file for append-only logging
 */
void initializeTripLog() {
  log("Initializing trip log file...");
  
  // Test if we can create/access the trip log file
  File testFile = LittleFS.open("/trip_log.bin", "a");
  if (testFile) {
    testFile.close();
    log("Trip log file ready for append-only logging");
  } else {
    log("Failed to access trip log file");
  }
}

/**
 * Append datapoint directly to trip log file (bombproof persistence)
 */
void appendToTripLog(LogdataRow datapoint) {
  // Open file for each write to ensure data is saved immediately
  File tripFile = LittleFS.open("/trip_log.bin", "a");
  if (tripFile) {
    size_t written = tripFile.write((uint8_t*)&datapoint, sizeof(LogdataRow));
    tripFile.flush(); // Immediate write to flash
    tripFile.close(); // Close immediately to ensure data is saved
    
    if (written == sizeof(LogdataRow)) {
      // Success - minimal logging to avoid stack issues
      static int writeCount = 0;
      writeCount++;
      if (writeCount % 10 == 0) {
        String writeMsg = "Trip log writes: " + String(writeCount);
        log(writeMsg.c_str());
      }
    } else {
      log("Failed to write to trip log");
    }
  } else {
    log("Failed to open trip log for writing");
  }
}

/**
 * Fügt einen Datenpunkt zum Recent-Buffer hinzu UND speichert ihn persistent
 */
void addToRecentData(LogdataRow datapoint) {
  // Add to RAM buffer for live display
  recentData[recentIndex] = datapoint;
  recentIndex = (recentIndex + 1) % MAX_RECENT_POINTS;
  if (totalRecentPoints < MAX_RECENT_POINTS) {
    totalRecentPoints++;
  }
  
  // Immediately append to persistent trip log
  appendToTripLog(datapoint);
}

/**
 * Load recent data from trip log file to populate RAM buffer
 */
void loadRecentDataFromTripLog() {
  log("Loading recent data from trip log...");
  
  if (!LittleFS.exists("/trip_log.bin")) {
    log("No trip log file found, starting fresh");
    return;
  }
  
  File readFile = LittleFS.open("/trip_log.bin", "r");
  if (!readFile) {
    log("Failed to open trip log for reading");
    return;
  }
  
  // Get file size and calculate number of datapoints
  size_t fileSize = readFile.size();
  int totalDatapoints = fileSize / sizeof(LogdataRow);
  
  if (totalDatapoints == 0) {
    log("Trip log is empty");
    readFile.close();
    return;
  }
  
  // Load last MAX_RECENT_POINTS into RAM buffer
  int pointsToLoad = totalDatapoints > MAX_RECENT_POINTS ? MAX_RECENT_POINTS : totalDatapoints;
  int startOffset = (totalDatapoints - pointsToLoad) * sizeof(LogdataRow);
  
  readFile.seek(startOffset);
  
  for (int i = 0; i < pointsToLoad; i++) {
    if (readFile.read((uint8_t*)&recentData[i], sizeof(LogdataRow)) == sizeof(LogdataRow)) {
      // Successfully loaded
    } else {
      log("Error reading trip log data");
      break;
    }
  }
  
  totalRecentPoints = pointsToLoad;
  recentIndex = pointsToLoad % MAX_RECENT_POINTS;
  
  readFile.close();
  
  String loadMsg = "Loaded " + String(pointsToLoad) + " datapoints from trip log (total: " + String(totalDatapoints) + ")";
  log(loadMsg.c_str());
}

/**
 * Legacy function - no longer used in append-only system
 */
void compressToHistoricalData() {
  // This function is no longer used in the simplified append-only logging system
  log("compressToHistoricalData called but not implemented in append-only system");
}

/**
 * Lightweight data saving - only saves essential recent data to prevent stack overflow
 */
void saveLightweightData() {
  log("Saving lightweight data to LittleFS...");
  
  // Only save last 30 recent points to minimize memory usage (ESP32 optimized)
  int pointsToSave = totalRecentPoints > 30 ? 30 : totalRecentPoints;
  
  if (pointsToSave > 0) {
    File recentFile = LittleFS.open("/recent_light.bin", "w");
    if (recentFile) {
      // Write header info
      recentFile.write((uint8_t*)&pointsToSave, sizeof(int));
      
      // Write only the last N points in correct chronological order
      for (int i = 0; i < pointsToSave; i++) {
        int idx = (recentIndex - pointsToSave + i + MAX_RECENT_POINTS) % MAX_RECENT_POINTS;
        recentFile.write((uint8_t*)&recentData[idx], sizeof(LogdataRow));
      }
      recentFile.close();
      
      String saveMsg = "Lightweight data saved - " + String(pointsToSave) + " points";
      log(saveMsg.c_str());
    } else {
      log("Failed to save lightweight data");
    }
  } else {
    log("No data to save");
  }
  
  // Also save total uptime
  saveTotalUptime();
}

/**
 * Loads lightweight data from LittleFS after reboot
 */
void loadLightweightData() {
  log("Loading lightweight data from LittleFS...");
  
  // Initialize to safe defaults
  totalRecentPoints = 0;
  recentIndex = 0;
  
  if (LittleFS.exists("/recent_light.bin")) {
    File recentFile = LittleFS.open("/recent_light.bin", "r");
    if (recentFile) {
      int savedPoints = 0;
      
      size_t bytesRead = recentFile.read((uint8_t*)&savedPoints, sizeof(int));
      if (bytesRead == sizeof(int) && savedPoints > 0 && savedPoints <= 30) {
        
        // Load points in correct order
        for (int i = 0; i < savedPoints; i++) {
          recentFile.read((uint8_t*)&recentData[i], sizeof(LogdataRow));
        }
        
        totalRecentPoints = savedPoints;
        recentIndex = savedPoints % MAX_RECENT_POINTS;
        
        String loadMsg = "Loaded lightweight data - Total: " + String(totalRecentPoints) + " points";
        log(loadMsg.c_str());
      } else {
        log("Invalid lightweight data, starting fresh");
      }
      recentFile.close();
    } else {
      log("Failed to open lightweight data file");
    }
  } else {
    log("No lightweight data file found, starting fresh");
  }
}

/**
 * Legacy function - no longer used in append-only system
 */
void saveCompressedData() {
  // This function is no longer used in the simplified append-only logging system
  log("saveCompressedData called but not implemented in append-only system");
}

/**
 * Legacy function - no longer used in append-only system
 */
void loadCompressedData() {
  // This function is no longer used in the simplified append-only logging system
  log("loadCompressedData called but not implemented in append-only system");
}

/**
 * Gibt die letzten n Datenpunkte zurück (vereinfacht - nur recent data)
 */
LogdataRow* getLatestDataPoints(int count, String timeRange) {
  // For now, all requests return recent data
  // TODO: Implement different time ranges by reading from trip log file
  return getRecentData(count);
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
 * Legacy function - returns recent data instead
 */
LogdataRow* getHourlyData(int count) {
  // In the simplified system, return recent data
  log("getHourlyData called - returning recent data instead");
  return getRecentData(count);
}

/**
 * Legacy function - returns recent data instead
 */
LogdataRow* getHistoricalData(int count) {
  // In the simplified system, return recent data
  log("getHistoricalData called - returning recent data instead");
  return getRecentData(count);
}

/**
 * Gibt die Anzahl verfügbarer Datenpunkte zurück (vereinfacht)
 */
int getTotalDataPoints(String timeRange) {
  // For now, always return recent points count
  // TODO: Calculate total points from trip log file size
  return totalRecentPoints;
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
  
  // Initialize LittleFS and trip logging
  log("Initializing LittleFS...");
  if (!LittleFS.begin(true)) {
    log("LittleFS initialization failed!");
  } else {
    log("LittleFS initialized successfully");
  }
  
  // Load persisted data
  log("Loading persisted data...");
  loadTotalUptime();
  loadRecentDataFromTripLog();
  initializeTripLog();
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
  
  // Add to buffer AND save to trip log
  addToRecentData(testData);
  
  log("Test datapoint added to buffer and saved to trip log");
  
  String statusMsg = "Buffer status - Index: " + String(recentIndex) + ", Total: " + String(totalRecentPoints);
  log(statusMsg.c_str());
  
  // Skip initial save to prevent stack issues
  log("Skipping initial save to prevent stack overflow");

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
      
      // Add to buffer AND save to trip log
      addToRecentData(newData);
      
      lastDataLogTime = currentTime;
      
      String newPointMsg = "New datapoint added - Index: " + String(recentIndex) + ", Total: " + String(totalRecentPoints);
      log(newPointMsg.c_str());
      
              // Simple milestone logging (no saving needed - data is already persistent)
      if (totalRecentPoints % 100 == 0) {
        String milestoneMsg = "Milestone reached: " + String(totalRecentPoints) + " datapoints (auto-saved to trip log)";
        log(milestoneMsg.c_str());
      }
    }
    
    // No periodic saving needed - data is automatically persistent!
    
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
  
  // Initialize buffer variables to safe defaults
  recentIndex = 0;
  totalRecentPoints = 0;
  isDataloggerRunning = false;
  
  log("Buffer variables initialized");
  
  // Erstelle Task auf Core 1 (nicht Core 0, da dort der Webserver läuft)
  BaseType_t taskResult = xTaskCreatePinnedToCore(
    dataloggerTask,        // Task-Funktion
    "DataloggerTask",      // Task-Name
    32000,                 // Stack-Größe (Bytes) - Maximum für ESP32
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
