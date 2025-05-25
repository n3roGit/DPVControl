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
  float batteryVoltage;
  float current;
  float rpm;
  float dutyCycle;
  float temperature;
  float humidity;
};

// Buffer für Datenpunkte im Speicher (für Webinterface-Graphen)
#define MAX_DATA_POINTS 600 // Speichert Daten für 10 Minuten bei 1-Sekunden-Intervall (reduziert von 3600)
extern LogdataRow dataPoints[MAX_DATA_POINTS];
extern int dataPointIndex;
extern int totalDataPoints;

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
LogdataRow* getLatestDataPoints(int count);
void deleteOldestLogFile();
int countLogFiles();
void saveTotalUptime();
void loadTotalUptime();
unsigned long getTotalUptime();

#endif