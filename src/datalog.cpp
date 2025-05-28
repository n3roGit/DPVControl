#include "datalog.h"
#include "log.h"
#include <FS.h>
#include <LittleFS.h>
#include "motor.h"
#include "main.h"
#include "battery.h"

// Session management
String currentSessionFile = "";
const String SESSION_DIR = "/datalog";
int currentSessionSplit = 0; // Current split number (0 = no split, 1+ = split parts)
const size_t MAX_SESSION_SIZE = 1048576; // 1MB max per session file to prevent memory issues

/**
 * Generate a unique filename for a new session using sequential numbering
 * If splitPart > 0, creates a split session like "session_0001-02.bin"
 */
String generateSessionFilename(int splitPart) {
    // Find the highest existing session number
    int maxSessionNumber = 0;
    
    // List existing sessions to find highest number
    File root = LittleFS.open(SESSION_DIR);
    if (root && root.isDirectory()) {
        File file = root.openNextFile();
        while (file) {
            String filename = String(file.name());
            if (filename.startsWith("session_") && filename.endsWith(".bin")) {
                // Extract number from filename like "session_0001.bin" or "session_0001-02.bin"
                String numberPart = filename.substring(8); // Remove "session_"
                
                // Handle split sessions (e.g., "0001-02.bin")
                int dashPos = numberPart.indexOf('-');
                if (dashPos != -1) {
                    numberPart = numberPart.substring(0, dashPos); // Take only the base number
                }
                
                // Remove ".bin" extension
                if (numberPart.endsWith(".bin")) {
                    numberPart = numberPart.substring(0, numberPart.length() - 4);
                }
                
                int sessionNumber = numberPart.toInt();
                if (sessionNumber > maxSessionNumber) {
                    maxSessionNumber = sessionNumber;
                }
            }
            file = root.openNextFile();
        }
        root.close();
    }
    
    // Create next session number with 4-digit padding
    int nextSessionNumber = maxSessionNumber + 1;
    String paddedNumber = String(nextSessionNumber);
    while (paddedNumber.length() < 4) {
        paddedNumber = "0" + paddedNumber;
    }
    
    String filename = SESSION_DIR + "/session_" + paddedNumber;
    
    // Add split part suffix if needed
    if (splitPart > 0) {
        String splitSuffix = String(splitPart);
        while (splitSuffix.length() < 2) {
            splitSuffix = "0" + splitSuffix;
        }
        filename += "-" + splitSuffix;
    }
    
    filename += ".bin";
    return filename;
}

/**
 * Create a new session file and directory if needed
 * If forceNewSession is false, it may create a split of the current session
 */
void createNewSession(bool forceNewSession) {
    // Create session directory if it doesn't exist
    if (!LittleFS.exists(SESSION_DIR)) {
        if (LittleFS.mkdir(SESSION_DIR)) {
            log("Created session directory");
        } else {
            log("Failed to create session directory");
        }
    }

    if (forceNewSession) {
        // Create completely new session (ESP32 restart)
        currentSessionSplit = 0;
        currentSessionFile = generateSessionFilename();
        String msg = "Created new session: " + currentSessionFile;
        log(msg.c_str());
    } else {
        // Create a split of the current session (memory management)
        currentSessionSplit++;
        currentSessionFile = generateSessionFilename(currentSessionSplit);
        String msg = "Created session split " + String(currentSessionSplit) + ": " + currentSessionFile;
        log(msg.c_str());
    }
    
    // Log session creation for debugging
    String detailMsg = "Session created at uptime: " + String(millis()/1000) + " seconds";
    log(detailMsg.c_str());
}

/**
 * Get current session file
 */
String getCurrentSessionFile() {
    return currentSessionFile;
}

/**
 * List all session files with improved file descriptor management
 */
String* listSessionFiles(int* count) {
    *count = 0;
    static String files[50]; // Maximum 50 sessions stored

    // Add delay to prevent file descriptor exhaustion
    vTaskDelay(5 / portTICK_PERIOD_MS);

    File root = LittleFS.open(SESSION_DIR);
    if (!root || !root.isDirectory()) {
        return files;
    }

    File file = root.openNextFile();
    while (file && *count < 50) {
        if (!file.isDirectory() && String(file.name()).endsWith(".bin")) {
            files[*count] = String(file.name());
            (*count)++;
        }
        file.close(); // Explicitly close each file
        file = root.openNextFile();
        
        // Small delay between file operations
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }

    root.close(); // Explicitly close directory
    
    // Add delay after directory operations
    vTaskDelay(5 / portTICK_PERIOD_MS);

    return files;
}

/**
* Regularly saves data about the state of the vehicle to disc.
* Runs on a separate core to avoid affecting the main application.
*/

/*
* CONSTANTS
*/ 

const String HEADER = "timestamp,motor_temp,mosfet_temp,battery_voltage,input_current,motor_current,erpm,duty_cycle,temperature,humidity,battery_level,leak_sensor,led_state,total_uptime";
const String DATALOG_DIR = "/datalog";

// Multi-interval logging for better storage efficiency
const unsigned long DATALOG_INTERVAL_FAST = 5000;   // Fast sensors: 5s (eRPM, current, voltage, temps)
const unsigned long DATALOG_INTERVAL_SLOW = 30000;  // Slow sensors: 30s (battery level, environment)
const unsigned long DATALOG_INTERVAL = DATALOG_INTERVAL_FAST; // Main interval

const int MAX_LOG_FILES = 10; // Maximum number of log files
const double MAX_SPEED_RPM = 15800; // Maximum speed in eRPM. Speed of 100%, copied from motor.cpp

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

// Multi-interval logging tracking  
unsigned long lastFastLog = 0;      // Last time fast sensors were logged
unsigned long lastSlowLog = 0;      // Last time slow sensors were logged
LogdataRow lastSlowData;             // Cache for slow-changing data

bool isDataloggerRunning = false;

unsigned long bootTimeSeconds = 0;  // Boot time when system started
unsigned long lastSaveTime = 0;
const unsigned long SAVE_INTERVAL = 60000; // Back to 60 seconds for performance
const unsigned long RUNTIME_BACKUP_INTERVAL = 300000; // Backup every 5 minutes

/**
 * Validate runtime value for reasonable range
 */
bool isValidRuntime(unsigned long runtime) {
  // Runtime should be between 0 and 10 years (10 * 365 * 24 * 3600 = 315,360,000 seconds)
  // This prevents corruption from loading garbage values
  return runtime <= 315360000UL;
}

/**
 * Load total runtime from file with validation and backup recovery
 */
void loadTotalUptime() {
  unsigned long loadedRuntime = 0;
  bool loadedSuccessfully = false;
  
  // Try to load from primary file
  if (LittleFS.exists("/runtime.txt")) {
    File file = LittleFS.open("/runtime.txt", "r");
    if (file) {
      String runtimeStr = file.readString();
      runtimeStr.trim();
      loadedRuntime = runtimeStr.toInt();
      file.close();
      
      if (isValidRuntime(loadedRuntime)) {
        bootTimeSeconds = loadedRuntime;
        loadedSuccessfully = true;
        String loadMsg = "Total runtime loaded: " + String(bootTimeSeconds) + " seconds (" + String(bootTimeSeconds/3600) + " hours)";
        log(loadMsg.c_str());
      } else {
        String errorMsg = "Invalid runtime value: " + String(loadedRuntime) + " seconds, trying backup";
        log(errorMsg.c_str());
      }
    } else {
      log("Failed to open primary runtime file, trying backup");
    }
  }
  
  // Try backup file if primary failed
  if (!loadedSuccessfully && LittleFS.exists("/runtime_backup.txt")) {
    File backupFile = LittleFS.open("/runtime_backup.txt", "r");
    if (backupFile) {
      String runtimeStr = backupFile.readString();
      runtimeStr.trim();
      loadedRuntime = runtimeStr.toInt();
      backupFile.close();
      
      if (isValidRuntime(loadedRuntime)) {
        bootTimeSeconds = loadedRuntime;
        loadedSuccessfully = true;
        String backupMsg = "Runtime recovered from backup: " + String(bootTimeSeconds) + " seconds (" + String(bootTimeSeconds/3600) + " hours)";
        log(backupMsg.c_str());
      } else {
        String errorMsg = "Backup runtime also invalid: " + String(loadedRuntime) + " seconds";
        log(errorMsg.c_str());
      }
    }
  }
  
  // Fallback to 0 if everything failed
  if (!loadedSuccessfully) {
    bootTimeSeconds = 0;
    log("No valid runtime file found, starting at 0");
  }
}

/**
 * Save total runtime to file with atomic write and backup
 */
void saveTotalUptime() {
  unsigned long currentTotalRuntime = bootTimeSeconds + (millis() / 1000);
  
  // Validate before saving
  if (!isValidRuntime(currentTotalRuntime)) {
    String errorMsg = "ERROR: Refusing to save invalid runtime: " + String(currentTotalRuntime) + " seconds";
    log(errorMsg.c_str());
    return;
  }
  
  // Atomic write: write to temporary file first, then rename
  bool saveSuccessful = false;
  
  // Try to save to temporary file
  File tempFile = LittleFS.open("/runtime_temp.txt", "w");
  if (tempFile) {
    tempFile.println(currentTotalRuntime);
    tempFile.flush(); // Ensure data is written
    tempFile.close();
    
    // Verify the temporary file was written correctly
    File verifyFile = LittleFS.open("/runtime_temp.txt", "r");
    if (verifyFile) {
      String verifyStr = verifyFile.readString();
      verifyStr.trim();
      verifyFile.close();
      
      if (verifyStr.toInt() == currentTotalRuntime) {
        // Rename temp file to main file (atomic on most filesystems)
        if (LittleFS.exists("/runtime.txt")) {
          LittleFS.remove("/runtime.txt");
        }
        if (LittleFS.rename("/runtime_temp.txt", "/runtime.txt")) {
          saveSuccessful = true;
          String saveMsg = "Runtime saved atomically: " + String(currentTotalRuntime) + " seconds (" + String(currentTotalRuntime/3600) + " hours)";
          log(saveMsg.c_str());
        } else {
          log("ERROR: Failed to rename runtime temp file");
        }
      } else {
        log("ERROR: Runtime verification failed after write");
      }
    } else {
      log("ERROR: Failed to verify written runtime file");
    }
    
    // Clean up temp file if rename failed
    if (LittleFS.exists("/runtime_temp.txt")) {
      LittleFS.remove("/runtime_temp.txt");
    }
  } else {
    log("ERROR: Failed to create runtime temp file");
  }
  
  // Create backup file every 5 minutes
  static unsigned long lastBackupTime = 0;
  if (millis() - lastBackupTime >= RUNTIME_BACKUP_INTERVAL) {
    File backupFile = LittleFS.open("/runtime_backup.txt", "w");
    if (backupFile) {
      backupFile.println(currentTotalRuntime);
      backupFile.close();
      String backupMsg = "Runtime backup created: " + String(currentTotalRuntime) + " seconds";
      log(backupMsg.c_str());
    }
    lastBackupTime = millis();
  }
  
  if (!saveSuccessful) {
    log("ERROR: Failed to save runtime - data may be lost on power failure!");
  }
}

/**
 * Get total runtime in seconds (simple calculation)
 */
unsigned long getTotalUptime() {
  return bootTimeSeconds + (millis() / 1000);
}

/**
 * Opens a new CSV file for writing
 */
void openCSVFile() {
  // Short pause before file access
  vTaskDelay(10 / portTICK_PERIOD_MS);
  
  String filename;
  for(int i = 0; i < 100; i++) { // Limit to prevent infinite loop
    filename = DATALOG_DIR + "/data_" + String(i) + ".csv";
    if (!LittleFS.exists(filename)) break;
    // Short pause during file search
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
  
  // Short pause after file writing
  vTaskDelay(10 / portTICK_PERIOD_MS);
}

/**
 * List all log files
 */
void listLogFiles() {
  String logMessage = "Listing directory: " + String(DATALOG_DIR);
  log(logMessage.c_str());

    File root = LittleFS.open(DATALOG_DIR);
  if(!root) {
    log("- Could not open directory");
      return;
  }
  if(!root.isDirectory()) {
    log("- Is not a directory");
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
 * Count the number of log files
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
 * Delete the oldest log file
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
        // Extract index from filename
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
      String deleteMessage = "Oldest file deleted: " + oldestFile;
      log(deleteMessage.c_str());
    } else {
      String errorMessage = "Error deleting file: " + oldestFile;
      log(errorMessage.c_str());
    }
  }
}

/**
 * Return the content of a log file
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
 * Return the path to the newest log file
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
        // Extract index from filename
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
 * Create optimized datapoint with smart interval logging
 */
LogdataRow createOptimizedDatapoint(unsigned long currentTime) {
  LogdataRow dp;
  dp.timestamp = currentTime;
  
  // Always log fast-changing critical data (5s interval)
  dp.batteryVoltage = getBatteryVoltage();
  dp.totalUptime = getTotalUptime();
  
  if (HAS_MOTOR) {
    dp.tempMotor = getVescUart().data.tempMotor;
    dp.tempMosfet = getVescUart().data.tempMosfet;
    dp.current = getVescUart().data.avgInputCurrent;
    dp.avgMotorCurrent = getVescUart().data.avgMotorCurrent;
    dp.erpm = getVescUart().data.rpm;
    dp.dutyCycle = getVescUart().data.dutyCycleNow;
  } else {
    // Fallback values if no motor
    dp.tempMotor = 25.0;
    dp.tempMosfet = 30.0;
    dp.current = 0.0;
    dp.avgMotorCurrent = 0.0;
    dp.erpm = 0.0;
    dp.dutyCycle = 0.0;
  }
  
  // Check if we need to update slow-changing data (30s interval)
  bool updateSlowData = (currentTime - lastSlowLog >= DATALOG_INTERVAL_SLOW);
  
  if (updateSlowData) {
    // Update slow-changing environmental data
    TempAndHumidity dhtData = dhtSensor.getTempAndHumidity();
    lastSlowData.temperature = isnan(dhtData.temperature) ? 0.0 : dhtData.temperature;
    lastSlowData.humidity = isnan(dhtData.humidity) ? 0.0 : dhtData.humidity;
    lastSlowData.batteryLevel = batteryLevel;
    lastSlowData.leakSensorState = leakSensorState;
    lastSlowData.ledState = 0; // TODO: Get real LED state
    lastSlowLog = currentTime;
  }
  
  // Use cached slow data for this datapoint
  dp.temperature = lastSlowData.temperature;
  dp.humidity = lastSlowData.humidity;
  dp.batteryLevel = lastSlowData.batteryLevel;
  dp.leakSensorState = lastSlowData.leakSensorState;
  dp.ledState = lastSlowData.ledState;
  
  return dp;
}

/**
 * Legacy function for compatibility - now calls optimized version
 */
LogdataRow createDatapoint() {
  return createOptimizedDatapoint(millis());
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
  file.print(datapoint.erpm);
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
 * Log current storage statistics
 */
void logStorageStats() {
  size_t totalBytes = LittleFS.totalBytes();
  size_t usedBytes = LittleFS.usedBytes();
  size_t freeBytes = totalBytes - usedBytes;
  
  String storageMsg = "Storage: " + String(freeBytes/1024) + "KB free / " + String(totalBytes/1024) + "KB total (" + String((freeBytes*100)/totalBytes) + "% free)";
  log(storageMsg.c_str());
  
  if (LittleFS.exists("/trip_log.bin")) {
    File tripFile = LittleFS.open("/trip_log.bin", "r");
    if (tripFile) {
      size_t fileSize = tripFile.size();
      int datapoints = fileSize / sizeof(LogdataRow);
      tripFile.close();
      
      String tripMsg = "Trip log: " + String(fileSize/1024) + "KB, " + String(datapoints) + " datapoints (" + String((datapoints*5)/60) + " min)";
      log(tripMsg.c_str());
    }
  }
}

/**
 * Check available storage space and clean up if necessary
 */
void checkAndCleanupStorage() {
  size_t totalBytes = LittleFS.totalBytes();
  size_t usedBytes = LittleFS.usedBytes();
  size_t freeBytes = totalBytes - usedBytes;
  
  // Keep at least 15% of total space free (minimum 100KB)
  size_t minFreeSpace = max(totalBytes / 7, (size_t)102400); // Changed from 10% to ~15%
  
  if (freeBytes < minFreeSpace) {
    log("Storage space running low, starting cleanup...");
    logStorageStats();
    
    // Check trip log file size
    if (LittleFS.exists("/trip_log.bin")) {
      File tripFile = LittleFS.open("/trip_log.bin", "r");
      if (tripFile) {
        size_t fileSize = tripFile.size();
        int totalDatapoints = fileSize / sizeof(LogdataRow);
        tripFile.close();
        
        if (totalDatapoints > 50000) { // Keep last 50000 datapoints (about 3+ days at 5s interval)
          log("Trimming trip log to preserve storage space...");
          
          // Read last datapoints in chunks to avoid memory issues
          const int keepDatapoints = 40000; // Keep 40000 points for safety margin (~2.3 days)
          const int chunkSize = 1000; // Process in 1000-point chunks
          
          File writeFile = LittleFS.open("/trip_log_temp.bin", "w");
          if (writeFile) {
            File readFile = LittleFS.open("/trip_log.bin", "r");
            if (readFile) {
              // Start from the position of the last keepDatapoints
              size_t startPos = (totalDatapoints - keepDatapoints) * sizeof(LogdataRow);
              readFile.seek(startPos);
              
              // Copy data in chunks
              LogdataRow* chunkBuffer = (LogdataRow*)malloc(chunkSize * sizeof(LogdataRow));
              if (chunkBuffer) {
                for (int chunk = 0; chunk < keepDatapoints; chunk += chunkSize) {
                  int pointsInChunk = min(chunkSize, keepDatapoints - chunk);
                  size_t bytesRead = readFile.read((uint8_t*)chunkBuffer, pointsInChunk * sizeof(LogdataRow));
                  
                  if (bytesRead == pointsInChunk * sizeof(LogdataRow)) {
                    writeFile.write((uint8_t*)chunkBuffer, bytesRead);
                  } else {
                    log("Error during chunked read in cleanup");
                    break;
                  }
                }
                free(chunkBuffer);
                
                readFile.close();
                writeFile.close();
                
                // Replace original file with trimmed version
                LittleFS.remove("/trip_log.bin");
                LittleFS.rename("/trip_log_temp.bin", "/trip_log.bin");
                
                String cleanupMsg = "Trip log trimmed from " + String(totalDatapoints) + " to " + String(keepDatapoints) + " datapoints";
                log(cleanupMsg.c_str());
              } else {
                log("Failed to allocate chunk buffer for cleanup");
                readFile.close();
                writeFile.close();
                LittleFS.remove("/trip_log_temp.bin");
              }
            } else {
              writeFile.close();
              LittleFS.remove("/trip_log_temp.bin");
              log("Failed to read original trip log during cleanup");
            }
          } else {
            log("Failed to create temporary file for cleanup");
          }
        }
      }
    }
    
    // Clean up other old files if they exist
    if (LittleFS.exists("/recent_light.bin")) {
      LittleFS.remove("/recent_light.bin");
      log("Removed old recent_light.bin file");
    }
    
    // Check final free space
    size_t finalFreeBytes = LittleFS.totalBytes() - LittleFS.usedBytes();
    String finalMsg = "Cleanup completed. Free space: " + String(finalFreeBytes) + " bytes";
    log(finalMsg.c_str());
  }
}

/**
 * Append datapoint directly to trip log file (bombproof persistence)
 */
void appendToTripLog(LogdataRow datapoint) {
  // Check storage space every 1000 writes (more efficient)
  static int writeCounter = 0;
  writeCounter++;
  if (writeCounter % 1000 == 0) {
    checkAndCleanupStorage();
  }
  
  // Log storage stats every 5000 writes (every ~7 hours at 5s interval)
  if (writeCounter % 5000 == 0) {
    logStorageStats();
  }
  
  // Open current session file for each write to ensure data is saved immediately
  if (currentSessionFile == "") {
    log("No current session file - creating new session");
    createNewSession();
  }
  
  // Check if current session file is getting too large
  if (LittleFS.exists(currentSessionFile)) {
    File sizeCheck = LittleFS.open(currentSessionFile, "r");
    if (sizeCheck) {
      size_t fileSize = sizeCheck.size();
      sizeCheck.close();
      
      if (fileSize > MAX_SESSION_SIZE) {
        String splitMsg = "Session file exceeded " + String(MAX_SESSION_SIZE/1024) + "KB, creating split";
        log(splitMsg.c_str());
        createNewSession(false); // Create split, not new session
      }
    }
  }
  
  // Log session file status periodically
  static int sessionWriteCount = 0;
  sessionWriteCount++;
  if (sessionWriteCount % 60 == 0) { // Every 5 minutes at 5s intervals
    String sessionMsg = "Session file: " + currentSessionFile + 
                       ", writes: " + String(sessionWriteCount) + 
                       ", age: " + String(millis()/1000) + "s" +
                       (currentSessionSplit > 0 ? ", split: " + String(currentSessionSplit) : "");
    log(sessionMsg.c_str());
  }
  
  // Add small delay to prevent file descriptor exhaustion
  vTaskDelay(2 / portTICK_PERIOD_MS);
  
  File tripFile = LittleFS.open(currentSessionFile, "a");
  if (tripFile) {
    size_t written = tripFile.write((uint8_t*)&datapoint, sizeof(LogdataRow));
    tripFile.flush(); // Immediate write to flash
    tripFile.close(); // Close immediately to ensure data is saved
    
    // Small delay after closing to ensure proper cleanup
    vTaskDelay(2 / portTICK_PERIOD_MS);
    
    if (written == sizeof(LogdataRow)) {
      // Success - minimal logging to avoid stack issues
      static int writeCount = 0;
      writeCount++;
      if (writeCount % 10 == 0) {
        String writeMsg = "Session log writes: " + String(writeCount) + 
                         " to " + currentSessionFile;
        log(writeMsg.c_str());
      }
    } else {
      String errorMsg = "Failed to write to session log: " + currentSessionFile + 
                       " (wanted: " + String(sizeof(LogdataRow)) + 
                       ", wrote: " + String(written) + ")";
      log(errorMsg.c_str());
    }
  } else {
    String errorMsg = "Failed to open session log for writing: " + currentSessionFile;
    log(errorMsg.c_str());
    // Add longer delay when file opening fails to prevent rapid retries
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

/**
 * Adds a data point to the Recent buffer AND saves it persistently
 */
void addToRecentData(LogdataRow datapoint) {
  // Add to RAM buffer for live display (always keep full resolution in RAM)
  recentData[recentIndex] = datapoint;
  recentIndex = (recentIndex + 1) % MAX_RECENT_POINTS;
  if (totalRecentPoints < MAX_RECENT_POINTS) {
    totalRecentPoints++;
  }
  
  // Use delta compression for persistent storage
  appendToTripLogCompressed(datapoint);
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
 * Returns the last n data points - reads from Trip-Log if more than RAM buffer
 */
LogdataRow* getLatestDataPoints(int count, String timeRange) {
  if (count <= totalRecentPoints) {
    // If we have enough data in RAM buffer, use it
    return getRecentData(count);
  }
  
  // If we need more data than in RAM, read from trip log file
  if (!LittleFS.exists("/trip_log.bin")) {
    return getRecentData(count);
  }
  
  File tripFile = LittleFS.open("/trip_log.bin", "r");
  if (!tripFile) {
    return getRecentData(count);
  }
  
  size_t fileSize = tripFile.size();
  int totalDatapoints = fileSize / sizeof(LogdataRow);
  
  if (totalDatapoints == 0) {
    tripFile.close();
    return getRecentData(count);
  }
  
  // Limit to available data
  int actualCount = count > totalDatapoints ? totalDatapoints : count;
  
  static LogdataRow extendedResult[500]; // Larger buffer for extended data
  if (actualCount > 500) actualCount = 500; // Safety limit
  
  // Read last actualCount datapoints from file
  size_t startPos = (totalDatapoints - actualCount) * sizeof(LogdataRow);
  tripFile.seek(startPos);
  
  for (int i = 0; i < actualCount; i++) {
    if (tripFile.read((uint8_t*)&extendedResult[i], sizeof(LogdataRow)) != sizeof(LogdataRow)) {
      log("Error reading extended trip data");
      tripFile.close();
      return getRecentData(count);
    }
  }
  
  tripFile.close();
  
  String extendedMsg = "Returning " + String(actualCount) + " datapoints from trip log (total: " + String(totalDatapoints) + ")";
  log(extendedMsg.c_str());
  
  return extendedResult;
}

/**
 * Returns Recent data (1s resolution)
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
 * Return the number of available data points (simplified)
 */
int getTotalDataPoints(String timeRange) {
  // For now, always return recent points count
  // TODO: Calculate total points from trip log file size
  return totalRecentPoints;
}

/**
 * Main task for the datalogger, runs on Core 1
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
  testData.erpm = 500.0;
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
  unsigned long sessionStartTime = millis();
  bool motorWasRunning = false;
  
  // Save total runtime more frequently and on important events
  static MotorState lastMotorState = standby;
  static unsigned long lastMotorStateChange = 0;
  
  while (true) {
    unsigned long currentTime = millis();
    
    // Debug every 10 seconds to show we're alive
    static unsigned long lastDebugTime = 0;
    if (currentTime - lastDebugTime >= 10000) {
      String aliveMsg = "Datalogger task alive - Total points: " + String(totalRecentPoints) + 
                       ", Session: " + currentSessionFile + 
                       ", Session age: " + String((currentTime - sessionStartTime)/1000) + "s";
      log(aliveMsg.c_str());
      lastDebugTime = currentTime;
    }
    
    // Sessions now span entire ESP32 uptime - no automatic session splits
    // A new session is only created at ESP32 startup (handled in datalogSetup)
    // If memory becomes an issue, we'll split sessions with numbered suffixes
    
    // Create new datapoint every 5 seconds with optimized multi-interval logging
    if (currentTime - lastDataLogTime >= DATALOG_INTERVAL) {
      // Create optimized datapoint with smart sensor intervals
      LogdataRow newData = createOptimizedDatapoint(currentTime);
      
      // Add to buffer AND save to trip log
      addToRecentData(newData);
      
      lastDataLogTime = currentTime;
      
      // Log occasionally to show activity (every 10th datapoint = ~50s)
      static int logCounter = 0;
      logCounter++;
      if (logCounter % 10 == 0) {
        String newPointMsg = "Datapoint #" + String(logCounter) + " - Runtime: " + String(getTotalUptime()/60) + " min, " + String(totalRecentPoints) + " in buffer";
        log(newPointMsg.c_str());
      }
    }
    
    // Save total runtime more frequently and on important events
    if (motorState != lastMotorState) {
      String stateChangeMsg = "Motor state changed from " + String(lastMotorState) + " to " + String(motorState) + " - saving runtime";
      log(stateChangeMsg.c_str());
      saveTotalUptime();
      lastMotorState = motorState;
      lastMotorStateChange = currentTime;
    }
    
    // Save runtime every 10 seconds (reduced from 60 seconds)
    if (currentTime - lastSaveTime >= SAVE_INTERVAL) {
      saveTotalUptime();
      lastSaveTime = currentTime;
    }
    
    // Also save runtime immediately when motor has been running for a while
    // This catches cases where power is lost during operation
    static unsigned long lastMotorRuntimeSave = 0;
    if ((motorState == on || motorState == cruise || motorState == turbo) && 
        currentTime - lastMotorRuntimeSave >= 30000) { // Every 30 seconds during motor operation (reduced from 5s for performance)
      saveTotalUptime();
      lastMotorRuntimeSave = currentTime;
    }
    
    // Keep task alive
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

/**
 * Setup function for the datalogger
 * Starts the datalogger task on Core 1
 */
void datalogSetup() {
  log("=== DATALOG SETUP START ===");
  
  // Initialize buffer variables to safe defaults
  recentIndex = 0;
  totalRecentPoints = 0;
  isDataloggerRunning = false;
  
  // Create new session
  createNewSession();
  
  log("Buffer variables initialized");
  
  // Create task on Core 1 (not Core 0, where the webserver runs)
  BaseType_t taskResult = xTaskCreatePinnedToCore(
    dataloggerTask,        // Task function
    "DataloggerTask",      // Task name
    32000,                 // Stack size (bytes) - Maximum for ESP32
    NULL,                  // Task parameters
    1,                     // Task priority (1 is low)
    &dataloggerTaskHandle, // Task handle
    1                      // Core ID (1)
  );
  
  if (taskResult == pdPASS) {
    log("Datalogger-Task SUCCESSFULLY created on Core 1");
  } else {
    log("ERROR: Failed to create Datalogger-Task!");
  }
  
  log("=== DATALOG SETUP END ===");
}

/**
 * Loop function for the datalogger
 * This is called from the main loop but does nothing,
 * since the actual datalogger runs on a different core
 */
void datalogLoop() {
  // The actual datalogger runs in a separate task,
  // nothing to do here.
}

/**
 * DELTA COMPRESSION SYSTEM
 * Only saves datapoints when significant changes occur
 */

// Global variables for delta compression
LogdataRow lastSavedData;
bool hasLastSavedData = false;

/**
 * Check if a datapoint should be saved based on delta thresholds
 */
bool shouldSaveDatapoint(LogdataRow& newData, LogdataRow& lastData) {
  if (!hasLastSavedData) return true; // Always save first datapoint
  
  // Check each parameter against its threshold
  if (abs(newData.tempMotor - lastData.tempMotor) > TEMP_THRESHOLD) return true;
  if (abs(newData.tempMosfet - lastData.tempMosfet) > TEMP_THRESHOLD) return true;
  if (abs(newData.batteryVoltage - lastData.batteryVoltage) > VOLTAGE_THRESHOLD) return true;
  if (abs(newData.current - lastData.current) > CURRENT_THRESHOLD) return true;
  if (abs(newData.avgMotorCurrent - lastData.avgMotorCurrent) > CURRENT_THRESHOLD) return true;
  if (abs(newData.erpm - lastData.erpm) > RPM_THRESHOLD) return true;
  if (abs(newData.dutyCycle - lastData.dutyCycle) > DUTY_THRESHOLD) return true;
  if (abs(newData.temperature - lastData.temperature) > TEMP_THRESHOLD) return true;
  if (abs(newData.humidity - lastData.humidity) > HUMIDITY_THRESHOLD) return true;
  
  // Check discrete values
  if (newData.batteryLevel != lastData.batteryLevel) return true;
  if (newData.leakSensorState != lastData.leakSensorState) return true;
  if (newData.ledState != lastData.ledState) return true;
  
  // Don't save if no significant changes
  return false;
}

/**
 * Interpolate between stored datapoints to create smooth timeline
 * Simplified for memory efficiency
 */
LogdataRow* interpolateData(LogdataRow* rawData, int rawCount, int targetCount) {
  if (!rawData || rawCount == 0 || targetCount == 0) return NULL;
  
  // Reduced buffer size to save memory
  static LogdataRow interpolatedData[200]; 
  if (targetCount > 200) targetCount = 200; // Reduced safety limit
  
  if (rawCount >= targetCount) {
    // If we have enough raw data, just copy it
    for (int i = 0; i < targetCount; i++) {
      interpolatedData[i] = rawData[i];
    }
    return interpolatedData;
  }
  
  // Simple linear interpolation
  float step = (float)(rawCount - 1) / (targetCount - 1);
  
  for (int i = 0; i < targetCount; i++) {
    float exactIndex = i * step;
    int baseIndex = (int)exactIndex;
    float ratio = exactIndex - baseIndex;
    
    if (baseIndex >= rawCount - 1) {
      interpolatedData[i] = rawData[rawCount - 1];
    } else {
      LogdataRow& before = rawData[baseIndex];
      LogdataRow& after = rawData[baseIndex + 1];
      
      // Simple interpolation for key values only
      interpolatedData[i].timestamp = before.timestamp + (long)((after.timestamp - before.timestamp) * ratio);
      interpolatedData[i].batteryVoltage = before.batteryVoltage + (after.batteryVoltage - before.batteryVoltage) * ratio;
      interpolatedData[i].current = before.current + (after.current - before.current) * ratio;
      interpolatedData[i].erpm = before.erpm + (after.erpm - before.erpm) * ratio;
      
      // Copy other values from closest point
      if (ratio < 0.5f) {
        interpolatedData[i].tempMotor = before.tempMotor;
        interpolatedData[i].tempMosfet = before.tempMosfet;
        interpolatedData[i].avgMotorCurrent = before.avgMotorCurrent;
        interpolatedData[i].dutyCycle = before.dutyCycle;
        interpolatedData[i].temperature = before.temperature;
        interpolatedData[i].humidity = before.humidity;
        interpolatedData[i].batteryLevel = before.batteryLevel;
        interpolatedData[i].leakSensorState = before.leakSensorState;
        interpolatedData[i].ledState = before.ledState;
        interpolatedData[i].totalUptime = before.totalUptime;
      } else {
        interpolatedData[i].tempMotor = after.tempMotor;
        interpolatedData[i].tempMosfet = after.tempMosfet;
        interpolatedData[i].avgMotorCurrent = after.avgMotorCurrent;
        interpolatedData[i].dutyCycle = after.dutyCycle;
        interpolatedData[i].temperature = after.temperature;
        interpolatedData[i].humidity = after.humidity;
        interpolatedData[i].batteryLevel = after.batteryLevel;
        interpolatedData[i].leakSensorState = after.leakSensorState;
        interpolatedData[i].ledState = after.ledState;
        interpolatedData[i].totalUptime = after.totalUptime;
      }
    }
  }
  
  return interpolatedData;
}

/**
 * Simple delta compression - only save when values change significantly
 */
void appendToTripLogCompressed(LogdataRow datapoint) {
  // Check if we should save this datapoint
  if (!shouldSaveDatapoint(datapoint, lastSavedData)) {
    // Skip saving if no significant changes
    static int skipCount = 0;
    skipCount++;
    if (skipCount % 50 == 0) {
      String skipMsg = "Delta compression: Skipped " + String(skipCount) + " unchanged datapoints (saving ~" + String((skipCount * 100) / (skipCount + 1)) + "% storage)";
      log(skipMsg.c_str());
    }
    return;
  }
  
  // Save this datapoint and update last saved data
  appendToTripLog(datapoint);
  lastSavedData = datapoint;
  hasLastSavedData = true;
  
  static int saveCount = 0;
  saveCount++;
  if (saveCount % 10 == 0) {
    log("Delta compression: Saved significant change");
  }
}
