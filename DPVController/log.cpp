#include "log.h"
#include "dht.h"
#include "main.h"
#include <uptime_formatter.h>
#include "motor.h"
#include "battery.h"


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
  log(label, value, EnableDebugLog);
}

void log(const char* label) {
  if (EnableDebugLog) {
    Serial.println(label);
  }
}

void logVehicleState() {
  if (loopCount % NormalLogOutputIntervall == 0) {
    Serial.println("---");
    Serial.print("bat lvl: ");
    Serial.println(batteryLevel);  // test battery level
    Serial.println("up " + uptime_formatter::getUptime());
    Serial.print("RPM: ");
    Serial.println(getVescUart().data.rpm);
    Serial.print("inpVoltage: ");
    Serial.println(getVescUart().data.inpVoltage);
    Serial.print("ampHours: ");
    Serial.println(getVescUart().data.ampHours);
    Serial.print("tempMosfet: ");
    Serial.println(getVescUart().data.tempMosfet);
    Serial.print("tempMotor: ");
    Serial.println(getVescUart().data.tempMotor);
    Serial.print("wattHours: ");
    Serial.println(getVescUart().data.wattHours);
    Serial.print("avgInputCurrent: ");
    Serial.println(getVescUart().data.avgInputCurrent);
    Serial.print("avgMotorCurrent: ");
    Serial.println(getVescUart().data.avgMotorCurrent);
    Serial.print("dutyCycleNow: ");
    Serial.println(getVescUart().data.dutyCycleNow);

    Serial.println("Temp: " + String(getTemp(), 2) + "°C");
    Serial.println("Humidity: " + String(getHuminity(), 1) + "%");

    Serial.println("---");
  }
}
