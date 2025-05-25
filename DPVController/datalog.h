#ifndef datalog_h
#define datalog_h

#include <FS.h>
#include <SPIFFS.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "motor.h"
#include "main.h"

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

// Simple append-only logging system for trip data
#define MAX_RECENT_POINTS 100     // RAM buffer for live display (5.6 KB)
// All data is immediately written to LittleFS for persistence

extern LogdataRow recentData[MAX_RECENT_POINTS];        // RAM buffer for live display

extern int recentIndex;
extern int totalRecentPoints;

// Persistence tracking
extern unsigned long lastHourlySave;
extern unsigned long lastHistoricalSave;

// Total uptime tracking
extern unsigned long totalUptimeSeconds;
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

// Simple append-only logging functions
void addToRecentData(LogdataRow datapoint);
void appendToTripLog(LogdataRow datapoint);
void initializeTripLog();
void loadRecentDataFromTripLog();

// Legacy functions
void deleteOldestLogFile();
int countLogFiles();
void saveTotalUptime();
void loadTotalUptime();
unsigned long getTotalUptime();

#endif