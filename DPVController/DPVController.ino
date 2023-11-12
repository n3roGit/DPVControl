/*
*   LIBRARIES
*/

#include "ClickButton.h"  //https://github.com/marcobrianza/ClickButton

#include "DHTesp.h"  //https://github.com/beegee-tokyo/DHTesp

#include <HardwareSerial.h>

#include <VescUart.h>  //https://github.com/RollingGecko/VescUartControl
VescUart UART;

#include "uptime_formatter.h"  //https://github.com/YiannisBourkelis/Uptime-Library

#include <Adafruit_NeoPixel.h>  //https://github.com/adafruit/Adafruit_NeoPixel


/*
*  CONSTANTS
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
const int LedBar_Num = 10;          // Number of LEDs in the strip
const int LedBar2_Num = 10;          // Number of LEDs in the strip
const int LEDBar_Brightness = 25;
const int LEDBar_BrightnessSecond = 1;

// Values for motorState
const int MOTOR_OFF = 0;
const int MOTOR_ON = 1;
const int MOTOR_STANDBY = 2;

const int MOTOR_MAX_SPEED = 14500;
const int MOTOR_MIN_SPEED = 6000;
const int SPEED_UP_TIME_MS = 5000 * 1000;    //time we want to take to  speed the motor from 0 to  full power.
const int SPEED_DOWN_TIME_MS = 300 * 1000;  //time we want to take to  speed the motor from full power to 0.
const int SPEED_STEPS = 10;                  //Number speed steps
const int MOTOR_SPEED_CHANGE = MOTOR_MAX_SPEED / SPEED_STEPS;
const int STANDBY_DELAY_MS = 60 /*s*/ * 1000 * 1000;  // Time until the motor goes into standby.
const bool EnableDebugLog = false;                    //Enable/Disable Serial Log
const float LED_Energy_Limiter = 0.8;
const int MotorButtonDelay = 500 * 1000;  //time befor button press the motor starts
const int StandbyBlinkStart = 15;         // Minutes for blink start
const int StandbyBlinkDuration = 10;      // Seconds between blink


// LED PWM parameters
const int LEDfrequency = 960;  // Initializing the integer variable 'LEDfrequency' as a constant at 4000 Hz. This sets the PWM signal frequency to 4000 Hz.
const int LEDresolution = 8;   // Initializing the integer variable 'LEDresolution' as a constant with 8-bit resolution. This defines the PWM signal resolution as 8 bits.
const int LEDchannel = 0;      // Initializing the integer variable 'LEDchannel' as a constant, set to 0 out of 16 possible channels. This designates the PWM channel as channel 0 out of a total of 16 channels.

// Constant for the number of cells in series
const int CellsInSeries = 13;
// Constant for the number of measurements used to calculate the average
const int batteryLevelMeasurements = 1000;

/*
*   GLOBAL VARIABLES
*/
Adafruit_NeoPixel strip = Adafruit_NeoPixel(LedBar_Num + LedBar2_Num, PIN_LedBar, NEO_GRB + NEO_KHZ800);
DHTesp dhtSensor;
int leftButtonState = 0;
int rightButtonState = 0;
int leakSensorState = 0;
int motorState = MOTOR_STANDBY;
int currentMotorSpeed = 0;           //Speed the motor is currently running at
int currentMotorStep = 1;
unsigned long currentMotorTime = 0;  //Time in MS when we last changed the currentMotorSpeed
int speedSetting = MOTOR_MIN_SPEED;  //The current speed setting. stays the same, even if motor is turned off.
int MOTOR_MAX_SPEED_TEMP;
int targetMotorSpeed = 0;  //The desired motor speed
unsigned long lastActionTime = 0;
int LED_State = 0;
int LED_State_Last = 0;
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
int NormalLogOutput = 0;
int NormalLogOutputIntervall = 1000;
int batteryAlerted = 0;
int FromTimeToTime = 0;
int FromTimeToTimeIntervall = 500;

int OverloadLimitMax = 40; // in Ampere
int OverloadLimit = OverloadLimitMax; // in Ampere

// Create ClickButton objects for the left and right buttons
ClickButton leftButton(PIN_LEFT_BUTTON, HIGH, CLICKBTN_PULLUP);
ClickButton rightButton(PIN_RIGHT_BUTTON, HIGH, CLICKBTN_PULLUP);


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

  // Set debounce and click times for buttons
  leftButton.debounceTime = 40;
  leftButton.multiclickTime = 300;
  leftButton.longClickTime = 1000;
  rightButton.debounceTime = 40;     //20
  rightButton.multiclickTime = 300;  //500
  rightButton.longClickTime = 1000;

  // Initialize serial communication
  Serial.begin(115200);

  // BEEP Initial
  Serial.println("Booting started...!");
  beep("1");

  // Setup DHT22 sensor
  dhtSensor.setup(PIN_DHT, DHTesp::DHT22);
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  Serial.println("Temp: " + String(data.temperature, 2) + "°C");
  Serial.println("Humidity: " + String(data.humidity, 1) + "%");
  Serial.println("---");

  // Initialize VESC UART communication
  Serial1.begin(115200, SERIAL_8N1, VESCRX, VESCTX);
  while (!Serial1) { ; }
  delay(500);
  UART.setSerialPort(&Serial1);
  delay(500);
  if (UART.getVescValues()) {
    Serial.println("Connected to VESC.");
  } else {
    Serial.println("Failed to connect to VESC.");
  }

  // Initialize LED PWM
  pinMode(PIN_LED, OUTPUT);                            //Setzt den GPIO-Pin 23 als Output (Ausgang)
  ledcSetup(LEDchannel, LEDfrequency, LEDresolution);  //Konfiguriert den PWM-Kanal 0 mit der Frequenz von 1 kHz und einer 8 Bit-Aufloesung
  ledcAttachPin(PIN_LED, LEDchannel);                  //Kopplung des GPIO-Pins 23 mit dem PWM-Kanal 0

  //Neopixel
  strip.begin();
  strip.show();  // Alle LEDs ausschalten
  setBarStandby();


  // Booting finished
  Serial.println("Booting finished!");
  // BEEP end
  beep("1");
}

void loop() {
  NormalLogOutput++;
  FromTimeToTime++;
  leftButton.Update();
  rightButton.Update();

  leftButtonState = digitalRead(PIN_LEFT_BUTTON);
  rightButtonState = digitalRead(PIN_RIGHT_BUTTON);

  log("rightButtonState", rightButtonState, EnableDebugLog);
  log("leftButtonState", leftButtonState, EnableDebugLog);
  //checkButtonClicks();
  updateSpeedSetting();
  controlStandby();
  controlMotor();
  controlLED();
  PreventOverload();
  checkForLeak();
  GetBatteryLevelInfo();
  GetVESCValues();
  normalLogOutput();
  FromTimeToTimeExecution();

  //Sonst ist der controller zu schnell durch den loop
  /*
  implemented this delay because the code is not working correct if this is not used. dont know why
  */
  delay(1);
}
