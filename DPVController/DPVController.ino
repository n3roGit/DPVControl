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



/*
*  CONSTANTS
*/
const int LedBar2_Num = 10; // (shared) Number of LEDs in the strip


// Button Values
const int PRESSED = 0;
const int DEPRESSED = 1;

//Lamp 
const int LAMP_OFF = 0;
const int LAMP_MAX = 4;


const int StandbyBlinkStart = 15;         // Minutes for blink start
const int StandbyBlinkDuration = 10;      // Seconds between blink

/*
*   GLOBAL VARIABLES
*/
DHTesp dhtSensor;
int LED_State = LAMP_OFF;

//Stuff below should be moved
unsigned long lastActionTime = 0;
unsigned long lastBlinkTime = 0;
unsigned long lastStandbyBlinkTime = 0;
unsigned long buttonPressStartTime = 0;
unsigned long leftButtonDownTime = 0;
unsigned long rightButtonDownTime = 0;
unsigned long StandbyBlinkWarningtime = (StandbyBlinkStart * 60 * 1000000);
int batteryAlerted = 0;
int FromTimeToTime = 0;

//Has to be here or the compiler puts it in weird places
struct LogdataRow {
  long time;
  float tempMotor;
};

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
