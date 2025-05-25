#include "main.h"

#include "constants.h"
#include "Blinker.h" 
#include "BlinkSequence.h" 
#include "log.h"
#include "beep.h"
#include "other.h"
#include "button.h"
#include "datalog.h"
#include "webserver.h"  // Include webserver header
#include "data_upload.h" // Include data upload header

int leakSensorState = 0;


int loopCount = 0;
int NormalLogOutputIntervall = 1000*10;

DHTesp dhtSensor;

/*
The Setup is chaotic. Needs a cleanup
*/
void setup() {
  pinMode(PIN_LEFT_BUTTON, INPUT);
  pinMode(PIN_RIGHT_BUTTON, INPUT);
  pinMode(PIN_LEAK_FRONT, INPUT_PULLUP);
  pinMode(PIN_LEAK_BACK, INPUT_PULLUP);
  pinMode(PIN_LAMP, OUTPUT);
  pinMode(PIN_BEEP, OUTPUT);

  // Initialize serial communication
  Serial.begin(115200);

  Serial.println("Booting started...!");


  buttonSetup();

  // Setup DHT22 sensor with error handling
  Serial.println("Initializing DHT22 sensor...");
  dhtSensor.setup(PIN_DHT, DHTesp::DHT22);
  delay(2000); // Give DHT sensor time to stabilize
  
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  if (isnan(data.temperature) || isnan(data.humidity)) {
    Serial.println("DHT22 sensor not ready, using default values");
    Serial.println("Temp: 20.0°C (default)");
    Serial.println("Humidity: 50.0% (default)");
  } else {
    Serial.println("Temp: " + String(data.temperature, 2) + "°C");
    Serial.println("Humidity: " + String(data.humidity, 1) + "%");
  }
  Serial.println("---");

  motorSetup();
  ledLampSetup();
  ledBarSetup();
  datalogSetup();
  batterySetup();

  // Initialize webserver on Core 0
  setupWebserver();

  // Booting finished
  Serial.println("Booting finished!");
  beep("1");
}

void loop() {
  long loopStart = millis();
  loopCount++;

  buttonLoop();
  motorLoop();
  checkForLeak();
  GetVESCValues();
  logVehicleState();
  FromTimeToTimeExecution();
  beepLoop();
  ledLampLoop();
  datalogLoop();

  long loopEnd = millis();
  long diff = loopEnd-loopStart;
  if (diff > 30){
    log("Loop took", diff, true);
  }
  
  delay(1);
}

