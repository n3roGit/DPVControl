/**
*
* Code that mostly just logs information to serial.
*
*/ 


// Function for logging with optional debugging delay
void log(const char* label, int value, boolean doLog) {
  if (doLog) {
    Serial.print(" ");
    Serial.print(label);
    Serial.print(": ");
    Serial.print(value);
    Serial.println();
  }
  if (EnableDebugLog) {
    delay(100);
  }
}


void logVehicleState() {
  if (loopCount % NormalLogOutputIntervall == 0) {
    Serial.println("---");

    Serial.print("bat lvl: ");
    Serial.println(batteryLevel);  // test battery level
    Serial.println("up " + uptime_formatter::getUptime());
    Serial.print("RPM: ");
    Serial.println(UART.data.rpm);
    Serial.print("inpVoltage: ");
    Serial.println(UART.data.inpVoltage);
    Serial.print("ampHours: ");
    Serial.println(UART.data.ampHours);
    Serial.print("tempMosfet: ");
    Serial.println(UART.data.tempMosfet);
    Serial.print("tempMotor: ");
    Serial.println(UART.data.tempMotor);
    Serial.print("wattHours: ");
    Serial.println(UART.data.wattHours);
    Serial.print("avgInputCurrent: ");
    Serial.println(UART.data.avgInputCurrent);
    Serial.print("avgMotorCurrent: ");
    Serial.println(UART.data.avgMotorCurrent);

    TempAndHumidity data = dhtSensor.getTempAndHumidity();
    Serial.println("Temp: " + String(data.temperature, 2) + "°C");
    Serial.println("Humidity: " + String(data.humidity, 1) + "%");

    Serial.println("---");
  }
}
