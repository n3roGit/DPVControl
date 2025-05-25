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

const String HEADER = "timestamp,motor_temp,battery_voltage,current,rpm,duty_cycle,temperature,humidity";
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
LogdataRow dataPoints[MAX_DATA_POINTS];
int dataPointIndex = 0;
int totalDataPoints = 0;
bool isDataloggerRunning = false;

/**
 * Öffnet eine neue CSV-Datei zum Schreiben
 */
void openCSVFile() {
  String filename;
  for(int i = 0; true; i++) {
    filename = DATALOG_DIR + "/data_" + String(i) + ".csv";
    if (!SPIFFS.exists(filename)) break;
  }
  csvFile = SPIFFS.open(filename, FILE_WRITE);
  if (EnableDebugLog) Serial.println(String("Schreibe in " + filename));
  csvFile.println(HEADER);
  csvFile.flush();
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
  
  // Motortemperatur
  if (HAS_MOTOR) {
    dp.tempMotor = getVescUart().data.tempMotor;
  } else {
    dp.tempMotor = 20.0 + (loopCount % 10);
  }
  
  // Batteriespannung
  dp.batteryVoltage = getBatteryVoltage();
  
  // Weitere VESC-Daten
  if (HAS_MOTOR) {
    // Verwenden wir die Eigenschaften von VescUart, die wir in getMotorPower() in motor.cpp sehen können
    dp.current = getVescUart().data.avgInputCurrent;
    dp.rpm = getVescUart().data.rpm;
    // Wir wissen nicht, ob duty_now oder dutyCycleNow der richtige Name ist,
    // daher berechnen wir einen simulierten Wert auf Basis anderer Werte
    dp.dutyCycle = (dp.rpm / MAX_SPEED_RPM) * 100.0;
  } else {
    // Simulierte Werte, falls kein Motor verfügbar
    dp.current = 5.0 + (loopCount % 20);
    dp.rpm = 1000 + (loopCount % 1000);
    dp.dutyCycle = 25.0 + (loopCount % 50);
  }
  
  // Umgebungstemperatur und Luftfeuchtigkeit von DHT-Sensor
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  dp.temperature = data.temperature;
  dp.humidity = data.humidity;
  
  return dp;
}

/**
 * Speichert einen Datenpunkt in die CSV-Datei
 */
void saveDatapoint(LogdataRow datapoint, File &file) {
  file.print(datapoint.timestamp);
  file.print(",");
  file.print(datapoint.tempMotor);
  file.print(",");
  file.print(datapoint.batteryVoltage);
  file.print(",");
  file.print(datapoint.current);
  file.print(",");
  file.print(datapoint.rpm);
  file.print(",");
  file.print(datapoint.dutyCycle);
  file.print(",");
  file.print(datapoint.temperature);
  file.print(",");
  file.print(datapoint.humidity);
  file.println();
  file.flush();
}

/**
 * Fügt einen Datenpunkt zum Kreis-Buffer hinzu
 */
void addDataPointToBuffer(LogdataRow datapoint) {
  dataPoints[dataPointIndex] = datapoint;
  dataPointIndex = (dataPointIndex + 1) % MAX_DATA_POINTS;
  if (totalDataPoints < MAX_DATA_POINTS) {
    totalDataPoints++;
  }
}

/**
 * Gibt die letzten n Datenpunkte zurück
 */
LogdataRow* getLatestDataPoints(int count) {
  if (count > totalDataPoints) count = totalDataPoints;
  if (count <= 0) return NULL;
  
  static LogdataRow result[MAX_DATA_POINTS];
  
  int start = (dataPointIndex - count + MAX_DATA_POINTS) % MAX_DATA_POINTS;
  for (int i = 0; i < count; i++) {
    result[i] = dataPoints[(start + i) % MAX_DATA_POINTS];
  }
  
  return result;
}

/**
 * Der Haupttask für den Datalogger, läuft auf Core 0
 */
void dataloggerTask(void *pvParameters) {
  log("Datalogger-Task gestartet auf Core 0");
  
  // Kurze Verzögerung nach dem Start
  vTaskDelay(10 / portTICK_PERIOD_MS);
  
  // Initialisiere SPIFFS, falls noch nicht geschehen
  if (!SPIFFS.begin(true)) {
    log("SPIFFS konnte nicht eingebunden werden");
    vTaskDelete(NULL);
    return;
  }
  
  // Verzögerung nach SPIFFS-Initialisierung
  vTaskDelay(10 / portTICK_PERIOD_MS);
  
  // Erstelle Verzeichnis, falls es nicht existiert
  if (!SPIFFS.exists(DATALOG_DIR)) {
    if (SPIFFS.mkdir(DATALOG_DIR)) {
      String dirMessage = "Verzeichnis " + String(DATALOG_DIR) + " erstellt";
      log(dirMessage.c_str());
    } else {
      String errorMessage = "Fehler beim Erstellen des Verzeichnisses " + String(DATALOG_DIR);
      log(errorMessage.c_str());
    }
    // Verzögerung nach Verzeichniserstellung
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
  
  // Entferne alte Log-Dateien, wenn zu viele existieren
  while (countLogFiles() >= MAX_LOG_FILES) {
    deleteOldestLogFile();
    // Verzögerung nach Dateilöschung
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
  
  // Öffne eine neue CSV-Datei
  openCSVFile();
  lastDataLogTime = millis();
  
  // Verzögerung nach Dateiöffnung
  vTaskDelay(10 / portTICK_PERIOD_MS);
  
  isDataloggerRunning = true;

  // Hauptschleife des Datalogger-Tasks
  while (true) {
    // Den Watchdog zurücksetzen
    if (millis() - lastDataLogTime >= DATALOG_INTERVAL) {
      // Erstelle und speichere einen neuen Datenpunkt
      LogdataRow data = createDatapoint();
      
      // Kurze Verzögerung für Watchdog
      vTaskDelay(1 / portTICK_PERIOD_MS);
      
      saveDatapoint(data, csvFile);
      
      // Kurze Verzögerung für Watchdog
      vTaskDelay(1 / portTICK_PERIOD_MS);
      
      addDataPointToBuffer(data);
      lastDataLogTime = millis();
    }
    
    // Längere Verzögerung zwischen den Intervallen, um Watchdog-Timer zu vermeiden
    // und anderen Tasks mehr Zeit zu geben
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

/**
 * Setup-Funktion für den Datalogger
 * Startet den Datalogger-Task auf Core 0
 */
void datalogSetup() {
  log("Starte Datalogger auf Core 0");
  
  // Erstelle Task auf Core 0
  xTaskCreatePinnedToCore(
    dataloggerTask,        // Task-Funktion
    "DataloggerTask",      // Task-Name
    8000,                  // Stack-Größe (Bytes)
    NULL,                  // Task-Parameter
    1,                     // Task-Priorität (1 ist niedrig)
    &dataloggerTaskHandle, // Task-Handle
    0                      // Core-ID (0)
  );
  
  log("Datalogger-Task erstellt auf Core 0");
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
