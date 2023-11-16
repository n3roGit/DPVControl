/**
*
* Code that mostly just logs information to serial.
*
*/ 


// Function for logging
void log(const char* label, int value, boolean doLog) {
  if (doLog) {
    Serial.print(" ");
    Serial.print(label);
    Serial.print(": ");
    Serial.print(value);
    Serial.println();
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
    Serial.println(UART.data.dutyCycleNow);

    TempAndHumidity data = dhtSensor.getTempAndHumidity();
    Serial.println("Temp: " + String(data.temperature, 2) + "°C");
    Serial.println("Humidity: " + String(data.humidity, 1) + "%");

    Serial.println("---");
  }
}
