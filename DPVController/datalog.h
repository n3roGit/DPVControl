#ifndef datalog_h
#define datalog_h

#include <FS.h>
#include <SPIFFS.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "motor.h"
#include "main.h"

// Session management
extern String currentSessionFile;
void createNewSession();
String generateSessionFilename();
String getCurrentSessionFile();
String* listSessionFiles(int* count);

// Task handle für den Datalogger
extern TaskHandle_t dataloggerTaskHandle;

struct LogdataRow {
  long timestamp;
  float tempMotor;
  float tempMosfet;
  float batteryVoltage;
  float current;
  float avgMotorCurrent;
  float rpm;
  float dutyCycle;
  float temperature;
  float humidity;
  int batteryLevel;
  int leakSensorState;
  int ledState;
  unsigned long totalUptime;
};

// Optimized logging system with longer sessions
#define MAX_RECENT_POINTS 300     // RAM buffer for live display - 25 minutes @ 5s intervals
// All data is immediately written to LittleFS for persistence

extern LogdataRow recentData[MAX_RECENT_POINTS];        // RAM buffer for live display

extern int recentIndex;
extern int totalRecentPoints;

// Persistence tracking
extern unsigned long lastHourlySave;
extern unsigned long lastHistoricalSave;

// Simple runtime tracking (like HeatControl)
extern unsigned long bootTimeSeconds;
extern bool isDataloggerRunning;

// Funktionsdeklarationen
void datalogSetup();
void datalogLoop();
void dataloggerTask(void *pvParameters);
void listLogFiles();
String getNewestLogFile();
String getLogFileContent(String filename);

// Multi-level data access functions
LogdataRow* getLatestDataPoints(int count, String timeRange = "recent");
LogdataRow* getRecentData(int count);
LogdataRow* getHourlyData(int count);
LogdataRow* getHistoricalData(int count);
int getTotalDataPoints(String timeRange = "recent");

// Optimized logging functions with multi-interval support
LogdataRow createOptimizedDatapoint(unsigned long currentTime);
void addToRecentData(LogdataRow datapoint);
void appendToTripLog(LogdataRow datapoint);
void initializeTripLog();
void loadRecentDataFromTripLog();
void checkAndCleanupStorage();
void logStorageStats();

// Legacy functions
void deleteOldestLogFile();
int countLogFiles();
void saveTotalUptime();
void loadTotalUptime();
unsigned long getTotalUptime();

#endif