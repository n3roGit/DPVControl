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

// Multi-level data storage for different time ranges (ESP32 DRAM optimized)
#define MAX_RECENT_POINTS 500     // Last 8.3 minutes - 1 second resolution (28 KB)
#define MAX_HOURLY_POINTS 48      // Last 48 minutes - 1 minute resolution (2.7 KB)
#define MAX_HISTORICAL_POINTS 24  // Last 2 hours - 5 minute resolution (1.3 KB)
// Total RAM usage: ~32 KB for data buffers

extern LogdataRow recentData[MAX_RECENT_POINTS];        // 1s resolution
extern LogdataRow hourlyData[MAX_HOURLY_POINTS];        // 1min resolution
extern LogdataRow historicalData[MAX_HISTORICAL_POINTS]; // 5min resolution

extern int recentIndex;
extern int hourlyIndex; 
extern int historicalIndex;
extern int totalRecentPoints;
extern int totalHourlyPoints;
extern int totalHistoricalPoints;

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

// Data management functions
void addToRecentData(LogdataRow datapoint);
void compressToHourlyData();
void compressToHistoricalData();
void saveCompressedData();
void loadCompressedData();

// Legacy functions
void deleteOldestLogFile();
int countLogFiles();
void saveTotalUptime();
void loadTotalUptime();
unsigned long getTotalUptime();

#endif