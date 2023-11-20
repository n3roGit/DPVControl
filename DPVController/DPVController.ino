/*
*   LIBRARIES
*/
#include "ClickButton.h"  //https://github.com/marcobrianza/ClickButton
#include "DHTesp.h"  //https://github.com/beegee-tokyo/DHTesp
#include <HardwareSerial.h>
#include <VescUart.h>  //https://github.com/SolidGeek/VescUart
#include "uptime_formatter.h"  //https://github.com/YiannisBourkelis/Uptime-Library
#include <Adafruit_NeoPixel.h>  //https://github.com/adafruit/Adafruit_NeoPixel

#include "FS.h" //Provided by framework
#include "SPIFFS.h"//Provided by framework

/*
* LOCAL C++ CODE
*/
#include "constants.h"
#include "all.h"
#include "Blinker.h" 
#include "BlinkSequence.h" 
#include "log.h"
#include "beep.h"
#include "other.h"
#include "button.h"
#include "datalog.h"


/*
*  CONSTANTS
*/


/*
*   GLOBAL VARIABLES
*/
DHTesp dhtSensor;

//Stuff below should be moved
unsigned long lastBlinkTime = 0;
unsigned long buttonPressStartTime = 0;
unsigned long leftButtonDownTime = 0;
unsigned long rightButtonDownTime = 0;
int FromTimeToTime = 0;

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

  // Setup DHT22 sensor
  dhtSensor.setup(PIN_DHT, DHTesp::DHT22);
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  Serial.println("Temp: " + String(data.temperature, 2) + "°C");
  Serial.println("Humidity: " + String(data.humidity, 1) + "%");
  Serial.println("---");

  motorSetup();
  ledLampSetup();
  ledBarSetup();
  datalogSetup();
  batterySetup();


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
