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


/*
*  PINS
*/
// https://wolles-elektronikkiste.de/en/programming-the-esp32-with-arduino-code
const int PIN_LEFT_BUTTON = 26;   // GPIO pin for the left button
const int PIN_RIGHT_BUTTON = 27;  // GPIO pin for the right button
const int PIN_LEAK_FRONT = 32;  // GPIO pin for front leak sensor
const int PIN_LEAK_BACK = 33;   // GPIO pin for back leak sensor
const int PIN_LAMP = 25;  // GPIO pin for LED
const int PIN_DHT = 14;  // GPIO pin for the buzzer
const int PIN_BEEP = 18;  //G18 OK
#define VESCRX 22  // GPIO pin for VESC UART RX
#define VESCTX 23  // GPIO pin for VESC UART TX
const int PIN_LEDBAR = 12; // Pin to which the LED strip is connected
const int PIN_POWERBANK = 13; // Pin to which the relay for power bank is connected


/*
*  CONSTANTS
*/
const int LedBar2_Num = 10; // (shared) Number of LEDs in the strip

enum MotorState {standby, on, off, cruise, turbo};

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
int leakSensorState = 0;
MotorState motorState = standby;
int LED_State = LAMP_OFF;
bool hasMotor = true;//Indicates that we have an actual motor plugged in.

//Stuff below should be moved
unsigned long lastActionTime = 0;
unsigned long lastBeepTime = 0;
unsigned long lastBlinkTime = 0;
unsigned long lastStandbyBeepTime = 0;
unsigned long lastStandbyBlinkTime = 0;
unsigned long buttonPressStartTime = 0;
unsigned long lastLeakBeepTime = 0;
unsigned long leftButtonDownTime = 0;
unsigned long rightButtonDownTime = 0;
unsigned long StandbyBlinkWarningtime = (StandbyBlinkStart * 60 * 1000000);
int batteryAlerted = 0;
int FromTimeToTime = 0;
int FromTimeToTimeIntervall = 500;

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
