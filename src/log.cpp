#include "log.h"
#include <DHTesp.h>
#include "main.h"
#include <uptime_formatter.h>
#include "motor.h"
#include "battery.h"
#include "vesc_task.h"

static bool shouldLogInfo() {
  return getLogLevel() >= 1;
}

static bool shouldLogDebug() {
  return getLogLevel() >= 2;
}

static bool shouldLogError() {
  return getLogLevel() >= 0;
}


void log(const char* label, int value, bool doLog) {
  if (doLog) {
    Serial.print(" ");
    Serial.print(label);
    Serial.print(": ");
    Serial.print(value);
    Serial.println();
  }
}

void log(const char* label, int value) {
  log(label, value, shouldLogDebug());
}

void log(const char* label) {
  if (shouldLogInfo()) {
    Serial.println(label);
  }
}

void log(const String& message) {
  if (shouldLogInfo()) {
    Serial.println(message);
  }
}

void logDebug(const char* label) {
  if (shouldLogDebug()) {
    Serial.println(label);
  }
}

void logDebug(const String& message) {
  if (shouldLogDebug()) {
    Serial.println(message);
  }
}

void logError(const char* label) {
  if (shouldLogError()) {
    Serial.println(label);
  }
}

void logError(const String& message) {
  if (shouldLogError()) {
    Serial.println(message);
  }
}

void logVehicleState() {
  if (shouldLogDebug() && loopCount % NormalLogOutputIntervall == 0) {
    // Get VESC data safely
    VescData vescData = getVescData();
    
    Serial.println("---");
    Serial.print("bat lvl: ");
    Serial.println(batteryLevel);  // test battery level
    Serial.println("up " + uptime_formatter::getUptime());
    Serial.print("eRPM: ");
    Serial.println(vescData.rpm);
    Serial.print("inpVoltage: ");
    Serial.println(vescData.inpVoltage);
    Serial.print("ampHours: ");
    Serial.println(vescData.ampHours);
    Serial.print("tempMosfet: ");
    Serial.println(vescData.tempMosfet);
    Serial.print("tempMotor: ");
    Serial.println(vescData.tempMotor);
    Serial.print("wattHours: ");
    Serial.println(vescData.wattHours);
    Serial.print("avgInputCurrent: ");
    Serial.println(vescData.avgInputCurrent);
    Serial.print("avgMotorCurrent: ");
    Serial.println(vescData.avgMotorCurrent);
    Serial.print("dutyCycleNow: ");
    Serial.println(vescData.dutyCycleNow);

    TempAndHumidity data = dhtSensor.getTempAndHumidity();
    Serial.println("Temp: " + String(data.temperature, 2) + "°C");
    Serial.println("Humidity: " + String(data.humidity, 1) + "%");

    Serial.println("---");
  }
}
