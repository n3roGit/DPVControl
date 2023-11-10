#include "ClickButton.h"  //https://github.com/marcobrianza/ClickButton

#include "DHTesp.h"  //https://github.com/beegee-tokyo/DHTesp

#include <HardwareSerial.h>

#include <VescUart.h>  //https://github.com/RollingGecko/VescUartControl
VescUart UART;

#include "uptime_formatter.h"  //https://github.com/YiannisBourkelis/Uptime-Library

#include <Adafruit_NeoPixel.h>  //https://github.com/adafruit/Adafruit_NeoPixel


/*
It still has to be checked if the currently used GPIOs are the optimal ones.
*/

// https://wolles-elektronikkiste.de/en/programming-the-esp32-with-arduino-code

const int PIN_LEFT_BUTTON = 26;   // GPIO pin for the left button
const int PIN_RIGHT_BUTTON = 27;  // GPIO pin for the right button

const int PIN_LEAK_FRONT = 32;  // GPIO pin for front leak sensor
const int PIN_LEAK_BACK = 33;   // GPIO pin for back leak sensor

const int PIN_LED = 25;  // GPIO pin for LED


const int PIN_DHT = 14;  // GPIO pin for the buzzer
DHTesp dhtSensor;

const int PIN_BEEP = 18;  //G18 OK

#define VESCRX 22  // GPIO pin for VESC UART RX
#define VESCTX 23  // GPIO pin for VESC UART TX

const int PIN_LedBar = 12;          // Pin to which the LED strip is connected
const int LedBar_Num = 10;          // Number of LEDs in the strip
const int LedBar2_Num = 10;          // Number of LEDs in the strip
const int LEDBar_Brightness = 25;
const int LEDBar_BrightnessSecond = 1;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(LedBar_Num + LedBar2_Num, PIN_LedBar, NEO_GRB + NEO_KHZ800);



// Values for motorState
const int MOTOR_OFF = 0;
const int MOTOR_ON = 1;
const int MOTOR_STANDBY = 2;


/*
Here please organize the variables smartly. here i use milliseconds in some places and minutes or seconds in others. how would you ideally do this?
*/
//Constants
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

//Variables
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

// Function to update the motor speed setting
void updateSpeedSetting() {
  if (motorState != MOTOR_STANDBY) {
    if (rightButton.clicks == -2) {
      log("rightButton.clicks", rightButton.clicks, true);
      rightButton.clicks = 0;
      speedSetting += MOTOR_SPEED_CHANGE;
      if (speedSetting > MOTOR_MAX_SPEED) {
        speedSetting = MOTOR_MAX_SPEED;
      }
      log("speedSetting", speedSetting, true);
      currentMotorStep = (currentMotorStep < 10) ? currentMotorStep + 1 : 10;
      setBarSpeed(currentMotorStep);
    }

    if (leftButton.clicks == -2) {
      log("leftButton.clicks", leftButton.clicks, true);
      speedSetting -= MOTOR_SPEED_CHANGE;
      if (speedSetting < MOTOR_MIN_SPEED) {
        speedSetting = MOTOR_MIN_SPEED;
      }
      log("speedSetting", speedSetting, true);
      currentMotorStep = (currentMotorStep > 1) ? currentMotorStep - 1 : 1;
      setBarSpeed(currentMotorStep);
    }
  }
}

// Function to control standby mode
void controlStandby() {
  if (motorState == MOTOR_STANDBY) {
    //Wake up from Standup
    if (leftButton.clicks == -2 || rightButton.clicks == -2) {
      motorState = MOTOR_OFF;
      log("leaving standby", 1, true);
      lastActionTime = micros();
      beep("2");
      setBarSpeed(currentMotorStep);

    }
  } else {
    if (lastActionTime + STANDBY_DELAY_MS < micros()) {
      //Go into standby
      log("going to standby", micros(), true);
      motorState = MOTOR_STANDBY;
      beep("2");
      setBarStandby();

    }
    if (leftButtonState == 0 || rightButtonState == 0) {
      //While not in standby, any button click updates the standby counter.
      lastActionTime = micros();
      log("update lastActionTime", lastActionTime, EnableDebugLog);
    }
  }
}

void controlMotor() {
  if (motorState != MOTOR_STANDBY) {
    if (leftButtonState == 0 || rightButtonState == 0) {
      motorState = MOTOR_ON;
    } else {
      motorState = MOTOR_OFF;
    }
  }
  log("motorstate", motorState, EnableDebugLog);

  if (motorState == MOTOR_STANDBY || motorState == MOTOR_OFF) {
    // Motor is off
    targetMotorSpeed = 0;
  } else if (motorState == MOTOR_ON) {
    targetMotorSpeed = speedSetting;
  }
  // Serial.print(" targetMotorSpeed: ");
  // Serial.print(targetMotorSpeed);

  setSoftMotorSpeed();
}

void controlLED() {
  if (rightButton.clicks == -3) {
    log("rightButton.clicks", rightButton.clicks, EnableDebugLog);

    switch (LED_State) {
      case 0:
        LED_State = 1;
        break;
      case 1:
        LED_State = 2;
        break;
      case 2:
        LED_State = 3;
        break;
      case 3:
        LED_State = 4;
        break;
      case 4:
        LED_State = 0;
        break;
      default:
        // If an invalid state is somehow reached, turn the LED off
        LED_State = 0;
        break;
    }
    setLEDState(LED_State);
    setBarLED(LED_State);
    log("LED_State", LED_State, true);
  }
}


void GetBatteryLevelInfo() {
  if (leftButton.clicks == -3) {
    log("leftButton.clicks", leftButton.clicks, EnableDebugLog);
    if (batteryLevel < 10) {
      beep("1");
    } else {
      // Determine how many full 10% steps have been reached
      int steps = batteryLevel / 10;

      // Generate a string with '1' for each full 10% step
      String beepSequence = "";
      for (int i = 0; i < steps; i++) {
        beepSequence += '2';
      }

      // If steps are present, call up the beep function
      if (steps > 0) {
        beep(beepSequence);
      }
    }
  }
}

void BatteryLevelAlert() {
  if (batteryLevel <= 30 && batteryLevel >= 21 && batteryAlerted != 30) {
    beep("222");  // Three long beeps at 30%
    log("BatteryAlert", batteryLevel, true);
    batteryAlerted = 30;  // Sets the status to 30%

  } else if (batteryLevel <= 20 && batteryLevel >= 11 && batteryAlerted != 20) {
    beep("22");  // Beep twice at 20%
    log("BatteryAlert", batteryLevel, true);
    batteryAlerted = 20;  // Sets the status to 20%

  } else if (batteryLevel <= 10 && batteryAlerted != 10) {
    beep("2");  // One beep at 10%
    log("BatteryAlert", batteryLevel, true);
    batteryAlerted = 10;  // Sets the status to 10%

  }
}
/*
is this a clever solution to prevent overload?
*/
void PreventOverload() {

  if (LED_State == 3) {
    OverloadLimit = OverloadLimitMax - 3;
  } else if (LED_State == 4) {
    OverloadLimit = OverloadLimitMax - 4;
  } else {
    OverloadLimit = OverloadLimitMax;
  }

/*
// something like this to prevent overload. so i can limit the motor speed if i have other devices consuming current
  if (UART.data.avgInputCurrent >= OverloadLimit) {
    speedSetting -= MOTOR_SPEED_CHANGE;
  }  else if (UART.data.avgInputCurrent < OverloadLimit) {
    speedSetting += MOTOR_SPEED_CHANGE;
  }
  */
}



/**
* Slowly changes the motor speed to targetMotorSpeed.
*
**/
void setSoftMotorSpeed() {

  float timePassedSinceLastChange = micros() - currentMotorTime;
  bool speedUp = currentMotorSpeed < targetMotorSpeed;

  if (speedUp) {
    float maxChange = timePassedSinceLastChange * MOTOR_MAX_SPEED / SPEED_UP_TIME_MS;
    currentMotorSpeed += maxChange;
    currentMotorSpeed = min(currentMotorSpeed, targetMotorSpeed);
  } else {
    float maxChange = timePassedSinceLastChange * MOTOR_MAX_SPEED / SPEED_DOWN_TIME_MS;
    currentMotorSpeed -= maxChange;
    currentMotorSpeed = max(currentMotorSpeed, targetMotorSpeed);
  }
  log("currentMotorSpeed", currentMotorSpeed, EnableDebugLog);
  bool SendStop = false;
  if(currentMotorSpeed == 0 && SendStop == false)
  {
    SendStop = true;
  }
  else
  {
      UART.setRPM(currentMotorSpeed);
      SendStop = false;
  }
  
  currentMotorTime = micros();
}

void setLEDState(int state) {

  int brightness;
  switch (state) {
    case 0:
      brightness = 0;
      break;
    case 1:
      brightness = 20;
      break;
    case 2:
      brightness = 76;
      break;
    case 3:
      brightness = 153;
      break;
    case 4:
      brightness = 255;
      break;
    default:
      // If an invalid state is provided, assume 0% brightness
      brightness = 0;
      break;
  }
  /*
Is it possible to change pwm frequency to advoid led flickering while filming 
*/
  //analogWrite(PIN_LED, brightness);  // LED-PIN, Brightness 0-255
  ledcWrite(LEDchannel, brightness);  // Set LED brightness using PWM
}




/*
Its working but while beeping the esp doesnt response
*/
void beep(const String& sequence) {
  for (char c : sequence) {
    int toneDuration = (c == '1') ? 200 : 600;
    digitalWrite(PIN_BEEP, HIGH);
    float startMicros = micros();
    while (micros() - startMicros < toneDuration * 1000) {
      // Wait until the desired duration is reached
    }
    digitalWrite(PIN_BEEP, LOW);
    lastBeepTime = micros();
    while (micros() - lastBeepTime < 400000) {
      // Pause between tones
    }
  }
}
/*
Its working but while blinking the esp doesnt response
*/
void blinkLED(const String& sequence) {
  for (char c : sequence) {
    setLEDState(0);
    int blinkDuration = (c == '1') ? 200 : 600;
    setLEDState(4);
    float startMicros = micros();
    while (micros() - startMicros < blinkDuration * 1000) {
      // Wait until the desired duration is reached
    }
    setLEDState(0);
    lastBlinkTime = micros();
    while (micros() - lastBlinkTime < 400000) {
      // Pause between blinking
    }
  }
  //todo: after blink set led to the last state of LED_State after 5 seconds
}



/*
only output. needs to be stored in database
*/
void GetVESCValues() {
  if (UART.getVescValues()) {
    /*
    Serial.print("RPM: ");
    Serial.println(UART.data.rpm);
    Serial.print("inpVoltage: ");
    Serial.println(UART.data.inpVoltage);
    Serial.print("ampHours: ");
    Serial.println(UART.data.ampHours);
    Serial.print("tachometerAbs: ");
    Serial.println(UART.data.tachometerAbs);
    */

    updateBatteryLevel(UART.data.inpVoltage);
  } else {
    log("Failed to get VESC data!", 00000, EnableDebugLog);
  }
}
void checkForLeak() {
  int frontLeakState = digitalRead(PIN_LEAK_FRONT);
  int backLeakState = digitalRead(PIN_LEAK_BACK);

  // Check whether one of the pins is "HIGH"
  if (frontLeakState == LOW || backLeakState == LOW) {
    leakSensorState = 1;  // There is a leak
    log("leakSensorState", leakSensorState, true);
    setBarLeak();    

  }
  log("frontLeakState", frontLeakState, EnableDebugLog);
  log("backLeakState", backLeakState, EnableDebugLog);
  log("frontLeakState", frontLeakState, EnableDebugLog);
}

void BeepForLeak() {
  if (leakSensorState == 1 && micros() - lastBeepTime >= (10 * 1000 * 1000)) {  // Every 10 seconds
    beep("22222");                                                              // Here is the desired sequence for the sound
    log("WARNING LEAK", 22222, true);
    lastLeakBeepTime = micros();  // update the time of the last call
  }
}
void BeepForStandby() {
  if (motorState == MOTOR_STANDBY && micros() - lastStandbyBeepTime >= (1 * 60 * 1000000)) {
    beep("1");  // Hier die gewünschte Sequenz für den Ton
    log("still in standby", 1, true);
    lastStandbyBeepTime = micros();  // Update the time of the last call
  }
}
void BlinkForLongStandby() {
  if (motorState == MOTOR_STANDBY && micros() - lastStandbyBlinkTime >= StandbyBlinkWarningtime && LED_State == 0) {
    blinkLED("111222111");  // Hier die gewünschte Sequenz für den Ton
    log("sos iam alone", 111222111, true);
    StandbyBlinkWarningtime = (StandbyBlinkDuration * 1000000);  //every 30 sec
    lastStandbyBlinkTime = micros();                             // Update the time of the last call
  } else {
    StandbyBlinkWarningtime = (StandbyBlinkStart * 60 * 1000000);  //every 3 minutes
  }
}

/*
idea to check all the click codes in one funtion. not working at the moment
*/
void checkButtonClicks() {
  leftButton.Update();
  rightButton.Update();


  if (rightButtonState == 0) {
    if (leftButton.clicks == 1) {
      Serial.println("1 Click - Hold");
    } else if (leftButton.clicks == 2) {
      Serial.println("2 Clicks - Hold");
    } else if (leftButton.clicks == 3) {
      Serial.println("3 Clicks - Hold");
    }
    if (leftButtonState == 0) {
      if (rightButton.clicks == 1) {
        Serial.println("Hold - 1 Click");
      } else if (rightButton.clicks == 2) {
        Serial.println("Hold - 2 Clicks");
      } else if (rightButton.clicks == 3) {
        Serial.println("Hold - 3 Clicks");
      }
    }
  }



  if (leftButton.clicks == 1 && rightButton.clicks == 1) {
    Serial.println("1 Click - 1 Click");
  } else if (leftButton.clicks == 2 && rightButton.clicks == 2) {
    Serial.println("2 Clicks - 2 Clicks");
  } else if (leftButton.clicks == 3 && rightButton.clicks == 3) {
    Serial.println("3 Clicks - 3 Clicks");
  } else if (leftButton.clicks == 1 && rightButtonState == 1) {
    Serial.println("1 Click -");
  } else if (rightButton.clicks == 1 && leftButtonState == 1) {
    Serial.println("- 1 Click");
  }
}





void setBar(int stripNumber, int numLEDsOn, String hexColorOn, int brightnessOn, String hexColorOff, int brightnessOff) {
  // Make sure that stripNumber is valid (1 for the first strip, 2 for the second strip)
  if (stripNumber != 1 && stripNumber != 2) {
    return; // Unauthorized value, do nothing
  }

  // Berechne den Startindex basierend auf stripNumber
  int startIndex = (stripNumber == 1) ? 0 : LedBar_Num;

  // Calculate the end index based on stripNumber
  int endIndex = (stripNumber == 1) ? LedBar_Num : LedBar_Num + LedBar2_Num;

  // Convert the hex color value to RGB color values for the switched-on color
  long numberOn = (long)strtol(&hexColorOn[1], NULL, 16);
  int redOn = numberOn >> 16;
  int greenOn = (numberOn >> 8) & 0xFF;
  int blueOn = numberOn & 0xFF;

  // Set the LEDs according to the specified brightness and colors
  for (int i = startIndex; i < startIndex + numLEDsOn; i++) {
    int dimmed_color_r = redOn * brightnessOn / 100;
    int dimmed_color_g = greenOn * brightnessOn / 100;
    int dimmed_color_b = blueOn * brightnessOn / 100;
    strip.setPixelColor(i, strip.Color(dimmed_color_r, dimmed_color_g, dimmed_color_b));
  }

  // Set the LEDs for the side that is switched off
  for (int i = startIndex + numLEDsOn; i < endIndex; i++) {
    // Convert the hex color value to RGB color values for the switched off color
    long numberOff = (long)strtol(&hexColorOff[1], NULL, 16);
    int redOff = numberOff >> 16;
    int greenOff = (numberOff >> 8) & 0xFF;
    int blueOff = numberOff & 0xFF;
    strip.setPixelColor(i, strip.Color(redOff * brightnessOff / 100, greenOff * brightnessOff / 100, blueOff * brightnessOff / 100));
  }

  strip.show();  // Update LED strips
}

void setBarStandby() {
    setBar(1,10,"#e38f09", LEDBar_BrightnessSecond, "#000000", 0);
}

void setBarSpeed(int num) {
    setBar(1,num,"#cb1bf2", LEDBar_Brightness, "#000000", 0);
}

void setBarBattery(int num) {
  int calc = LedBar_Num-num;
  setBar(2,calc,"#e30b0b", LEDBar_BrightnessSecond, "#0a9e08", LEDBar_Brightness);
}

void setBarLeak() {
    int frontLeakState = digitalRead(PIN_LEAK_FRONT);
    int backLeakState = digitalRead(PIN_LEAK_BACK);

    if (backLeakState == LOW && frontLeakState == LOW) {
      setBar(1,10,"#0000FF", LEDBar_Brightness, "#0000FF", 0);
    } else if (backLeakState == LOW) {
      setBar(1,5,"#0000FF", LEDBar_Brightness, "#0000FF", 0);
    } else if(frontLeakState == LOW) {
      setBar(1,5,"#0000FF", 0, "#0000FF", LEDBar_Brightness);
    }
}

void setBarLED(int num) {
    int calc = LedBar_Num-num;
    setBar(1,calc,"#000000", 0, "#FFFFFF", LEDBar_Brightness);
}


// make a map function for this mapiopenigrecord
void updateBatteryLevel(float voltage) {
  if (voltage >= 2.8) {
    float singleCellVoltages[] = {4.18, 4.1, 3.99, 3.85, 3.77, 3.58, 3.42, 3.33, 3.21, 3.00, 2.87};
    int singleCellPercentages[] = {100, 96, 82, 68, 58, 34, 20, 14, 8, 2, 0};

    for (int i = 0; i < sizeof(singleCellVoltages) / sizeof(singleCellVoltages[0]); i++) {
      if (voltage >= singleCellVoltages[i] * CellsInSeries) {
        // Interpolation
        if (i > 0) {
          float voltageRange = singleCellVoltages[i] * CellsInSeries - singleCellVoltages[i - 1] * CellsInSeries;
          int percentageRange = singleCellPercentages[i - 1] - singleCellPercentages[i];
          float voltageDifference = singleCellVoltages[i] * CellsInSeries - voltage;
          float interpolationFactor = voltageDifference / voltageRange;
          batteryLevel = singleCellPercentages[i] + interpolationFactor * percentageRange;
        } else {
          batteryLevel = singleCellPercentages[i];
        }
        break;
      }
    }
    // Ensure that the battery level is limited to the range [0, 100]
    batteryLevel = constrain(batteryLevel, 0, 100);
    Serial.println("mehr volt");
  } else {
    batteryLevel = 100;
  }
  int steps = (batteryLevel + 5) / LedBar2_Num;
  steps = constrain(steps, 0, LedBar2_Num - 1);
  setBarBattery(steps);
}



void normalLogOutput() {
  if (NormalLogOutput % NormalLogOutputIntervall == 0) {
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

void FromTimeToTimeExecution() {
  if (FromTimeToTime % FromTimeToTimeIntervall == 0) {
  BeepForLeak();
  BeepForStandby();
  BlinkForLongStandby();
  BatteryLevelAlert();
  }
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
