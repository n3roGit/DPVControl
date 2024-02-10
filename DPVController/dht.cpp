#include "dht.h"
#include "DHTesp.h"
#include "constants.h"

DHTesp dhtSensor;

float getTemp(){
  return dhtSensor.getTemperature();
}

float getHuminity(){
  return dhtSensor.getHumidity();
}


void dhtSetup(){
  dhtSensor.setup(PIN_DHT, DHTesp::DHT22);
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  Serial.println("Temp: " + String(data.temperature, 2) + "°C");
  Serial.println("Humidity: " + String(data.humidity, 1) + "%");
  Serial.println("---");
}
