/*
*   LIBRARIES
*/

#include "ClickButton.h"  //https://github.com/marcobrianza/ClickButton

#include "DHTesp.h"  //https://github.com/beegee-tokyo/DHTesp

#include <HardwareSerial.h>

#include <VescUart.h>  //https://github.com/SolidGeek/VescUart

#include "uptime_formatter.h"  //https://github.com/YiannisBourkelis/Uptime-Library

#include <Adafruit_NeoPixel.h>  //https://github.com/adafruit/Adafruit_NeoPixel




/*
*  PINS
*/

// It still has to be checked if the currently used GPIOs are the optimal ones.
// https://wolles-elektronikkiste.de/en/programming-the-esp32-with-arduino-code
const int PIN_LEFT_BUTTON = 26;   // GPIO pin for the left button
const int PIN_RIGHT_BUTTON = 27;  // GPIO pin for the right button
const int PIN_LEAK_FRONT = 32;  // GPIO pin for front leak sensor
const int PIN_LEAK_BACK = 33;   // GPIO pin for back leak sensor
const int PIN_LED = 25;  // GPIO pin for LED
const int PIN_DHT = 14;  // GPIO pin for the buzzer
const int PIN_BEEP = 18;  //G18 OK
#define VESCRX 22  // GPIO pin for VESC UART RX
#define VESCTX 23  // GPIO pin for VESC UART TX
const int PIN_LedBar = 12;          // Pin to which the LED strip is connected

/*
*  CONSTANTS
*/
const bool EnableDebugLog = true;                    //Enable/Disable Serial Log

const int LedBar2_Num = 10; // (shared) Number of LEDs in the strip


// Values for motorState
const int MOTOR_OFF = 0;
const int MOTOR_ON = 1;
const int MOTOR_STANDBY = 2;

// Button Values
const int PRESSED = 0;
const int DEPRESSED = 1;

const float LED_Energy_Limiter = 0.8;
const int StandbyBlinkStart = 15;         // Minutes for blink start
const int StandbyBlinkDuration = 10;      // Seconds between blink

// Constant for the number of cells in series
const int CellsInSeries = 13;
// Constant for the number of measurements used to calculate the average
const int batteryLevelMeasurements = 1000;

/*
*   GLOBAL VARIABLES
*/
DHTesp dhtSensor;
int leakSensorState = 0;
int motorState = MOTOR_STANDBY;
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
int batteryLevel = 0;
int loopCount = 0;
int NormalLogOutputIntervall = 1000*10;
int batteryAlerted = 0;
int FromTimeToTime = 0;
int FromTimeToTimeIntervall = 500;

/*
The Setup is chaotic. Needs a cleanup
*/
void setup() {
  pinMode(PIN_LEFT_BUTTON, INPUT);
  pinMode(PIN_RIGHT_BUTTON, INPUT);
  pinMode(PIN_LEAK_FRONT, INPUT_PULLUP);
  pinMode(PIN_LEAK_BACK, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BEEP, OUTPUT);

  // Initialize serial communication
  Serial.begin(115200);

  // BEEP Initial
  Serial.println("Booting started...!");
  beep("1");

  buttonSetup();

  // Setup DHT22 sensor
  dhtSensor.setup(PIN_DHT, DHTesp::DHT22);
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  Serial.println("Temp: " + String(data.temperature, 2) + "°C");
  Serial.println("Humidity: " + String(data.humidity, 1) + "%");
  Serial.println("---");

  motorSetup();
  ledSetup();

  // Booting finished
  Serial.println("Booting finished!");
  // BEEP end
  beep("1");
}

void loop() {
  long loopStart = millis();
  loopCount++;

  buttonLoop();
  motorLoop();
  PreventOverload();
  checkForLeak();
  GetVESCValues();
  logVehicleState();
  FromTimeToTimeExecution();
  beepLoop();

  long loopEnd = millis();
  long diff = loopEnd-loopStart;
  if (diff > 30){
    log("Loop took", diff, true);
  }
  
  delay(1);
}
