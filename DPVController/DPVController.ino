#include "ClickButton.h" //https://github.com/marcobrianza/ClickButton

#include "DHTesp.h" //https://github.com/beegee-tokyo/DHTesp

#include <HardwareSerial.h>

#include <VescUart.h> //https://github.com/RollingGecko/VescUartControl
VescUart UART;

#include "uptime_formatter.h" //https://github.com/YiannisBourkelis/Uptime-Library

#include <Adafruit_NeoPixel.h> //https://github.com/adafruit/Adafruit_NeoPixel

#include <Preferences.h>

/*
It still has to be checked if the currently used GPIOs are the optimal ones.
*/

// https://wolles-elektronikkiste.de/en/programming-the-esp32-with-arduino-code

const int PIN_LEFT_BUTTON = 27;   // GPIO pin for the left button
const int PIN_RIGHT_BUTTON = 35;  // GPIO pin for the right button

const int PIN_LEAK_FRONT = 32;  // GPIO pin for front leak sensor
const int PIN_LEAK_BACK = 33;   // GPIO pin for back leak sensor

const int PIN_LED = 25;  // GPIO pin for LED

const int PIN_DHT = 14;  // GPIO pin for the buzzer
DHTesp dhtSensor;

const int PIN_BEEP = 18;  //G18 OK

#define VESCRX 22  // GPIO pin for VESC UART RX
#define VESCTX 23  // GPIO pin for VESC UART TX

const int PIN_LEDBAR = 12;       // Pin, an dem der LED-Streifen angeschlossen ist
const int LEDBAR_NUM = 10;      // Anzahl der LEDs im Streifen
const int LEDBAR_BRIGHTNESS = 255;   // Maximale Helligkeit (0-255)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(LEDBAR_NUM, PIN_LEDBAR, NEO_GRB + NEO_KHZ800);



// Values for motorState
const int MOTOR_OFF = 0;
const int MOTOR_ON = 1;
const int MOTOR_STANDBY = 2;


/*
Here please organize the variables smartly. here i use milliseconds in some places and minutes or seconds in others. how would you ideally do this?
*/
//Constants
const int MOTOR_MAX_SPEED = 14000;
const int MOTOR_MIN_SPEED = 2000;
const int SPEED_UP_TIME_MS = 5000 * 1000;    //time we want to take to  speed the motor from 0 to  full power.
const int SPEED_DOWN_TIME_MS = 1000 * 1000;  //time we want to take to  speed the motor from full power to 0.
const int SPEED_STEPS = 10;                  //Number speed steps
const int MOTOR_SPEED_CHANGE = MOTOR_MAX_SPEED / SPEED_STEPS;
const int STANDBY_DELAY_MS = 60 /*s*/ * 1000 * 1000;  // Time until the motor goes into standby.
const bool EnableDebugLog = false;                    //Enable/Disable Serial Log
const float LED_Energy_Limiter = 0.8;
const int MotorButtonDelay = 500 * 1000; //time befor button press the motor starts
const int StandbyBlinkStart = 15; // Minutes for blink start
const int StandbyBlinkDuration = 10; // Seconds between blink

// LED PWM parameters
const int LEDfrequency = 960; // Initializing the integer variable 'LEDfrequency' as a constant at 4000 Hz. This sets the PWM signal frequency to 4000 Hz.
const int LEDresolution = 8;   // Initializing the integer variable 'LEDresolution' as a constant with 8-bit resolution. This defines the PWM signal resolution as 8 bits.
const int LEDchannel = 0;      // Initializing the integer variable 'LEDchannel' as a constant, set to 0 out of 16 possible channels. This designates the PWM channel as channel 0 out of a total of 16 channels.

// Constant for the number of cells in series
const int CellsInSeries = 13;
// Constant for the number of measurements used to calculate the average
const int batteryLevelMeasurements = 100;

//Variables
int leftButtonState = 0;
int rightButtonState = 0;
int leakSensorState = 0;
int motorState = MOTOR_STANDBY;
int currentMotorSpeed = 0;           //Speed the motor is currently running at
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
  pinMode(PIN_LED, OUTPUT);                           //Setzt den GPIO-Pin 23 als Output (Ausgang)
  ledcSetup(LEDchannel, LEDfrequency, LEDresolution);      //Konfiguriert den PWM-Kanal 0 mit der Frequenz von 1 kHz und einer 8 Bit-Aufloesung
  ledcAttachPin(PIN_LED, LEDchannel);                    //Kopplung des GPIO-Pins 23 mit dem PWM-Kanal 0

  //Neopixel
  strip.begin();
  strip.show();  // Alle LEDs ausschalten
  setBar(10, "#FF0000", "#000000");

  // Booting finished
  Serial.println("Booting finished!");
  // BEEP end
  beep("222");
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
      float percentageSpeed = ((float)(speedSetting - MOTOR_MIN_SPEED) / (MOTOR_MAX_SPEED - MOTOR_MIN_SPEED)) * 100.0;
      setBar(percentageSpeed, "#FF0000", "#000000");
    }

    if (leftButton.clicks == -2) {
      log("leftButton.clicks", leftButton.clicks, true);
      speedSetting -= MOTOR_SPEED_CHANGE;
      if (speedSetting < MOTOR_MIN_SPEED) {
        speedSetting = MOTOR_MIN_SPEED;
      }
      log("speedSetting", speedSetting, true);
      float percentageSpeed = ((float)(speedSetting - MOTOR_MIN_SPEED) / (MOTOR_MAX_SPEED - MOTOR_MIN_SPEED)) * 100.0;
      setBar(percentageSpeed, "#FF0000", "#000000");

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
    }
  } else {
    if (lastActionTime + STANDBY_DELAY_MS < micros()) {
      //Go into standby
      log("going to standby", micros(), true);
      motorState = MOTOR_STANDBY;
      beep("2");
    }
    if (leftButtonState == 0 || rightButtonState == 0) {
      //While not in standby, any button click updates the standby counter.
      lastActionTime = micros();
      log("update lastActionTime", lastActionTime, EnableDebugLog);
    }
  }
}


/*
tried to make a delay before motor starts by pressing button. not working.
*/
/*
void controlMotor() {
  if (motorState != MOTOR_STANDBY) {
    // Prüfen, ob eine der beiden Tasten gedrückt wird

    if (leftButtonState == 0 || rightButtonState == 0) {
      // Wenn eine Taste gedrückt wurde und der Timer noch nicht gestartet ist, starten Sie ihn.
      log("leftButtonState", leftButtonState, EnableDebugLog);
      log("rightButtonState", rightButtonState, EnableDebugLog);
      log("buttonPressStartTime", buttonPressStartTime, EnableDebugLog);
      if (buttonPressStartTime == 0) {
        buttonPressStartTime = micros();
        log("buttonPressStartTime gesetzt", buttonPressStartTime, EnableDebugLog);
      }

      // Prüfen, ob die Dauer des Tastendrucks MotorButtonDelay Mikrosekunden erreicht hat
      if (micros() - buttonPressStartTime >= MotorButtonDelay) {
        motorState = MOTOR_ON;
        log("motorState", motorState, EnableDebugLog);
        log("MOTOR_ON", MOTOR_ON, EnableDebugLog);
      }
    } else {
      // Wenn keine Taste gedrückt wird, setzen Sie den Timer zurück.
      buttonPressStartTime = 0;
      motorState = MOTOR_OFF;
      log("buttonPressStartTime", buttonPressStartTime, EnableDebugLog);
      log("motorState", motorState, EnableDebugLog);
    }
  }

  log("motorstate", motorState, EnableDebugLog);

  if (motorState == MOTOR_STANDBY || motorState == MOTOR_OFF) {
    // Motor ist aus
    targetMotorSpeed = 0;
  } else if (motorState == MOTOR_ON) {
    targetMotorSpeed = speedSetting;
  }

  setSoftMotorSpeed();
}*/



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
    log("LED_State", LED_State, true);
  }
}

/*
is this a clever solution to prevent overload?
*/
void PreventOverload() {
  // Hinzugefügte Logik zur Überprüfung der Geschwindigkeit und LED_State
  if (speedSetting > int(MOTOR_MAX_SPEED * LED_Energy_Limiter) && LED_State >= 3) {
    LED_State_Last = LED_State;
    LED_State = 2;
    setLEDState(LED_State);
  }
  /*
  else if (speedSetting <= int(MOTOR_MAX_SPEED * LED_Energy_Limiter) && LED_State != 0 && LED_State != LED_State_Last) {
    LED_State = LED_State_Last;
    setLEDState(LED_State);
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
  //servo.write(currentMotorSpeed);
  UART.setRPM(currentMotorSpeed);
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
  ledcWrite(LEDchannel, brightness); // Set LED brightness using PWM
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
    log("Failed to get VESC data!", 00000, true);
  }
}
void checkForLeak() {
  int frontLeakState = digitalRead(PIN_LEAK_FRONT);
  int backLeakState = digitalRead(PIN_LEAK_BACK);

  // Überprüfen, ob einer der Pins auf "HIGH" ist
  if (frontLeakState == LOW || backLeakState == LOW) {
    leakSensorState = 1;  // Es liegt ein Leak vor
    log("leakSensorState", leakSensorState, true);
  }
  log("frontLeakState", frontLeakState, EnableDebugLog);
  log("backLeakState", backLeakState, EnableDebugLog);
  log("frontLeakState", frontLeakState, EnableDebugLog);
}

void BeepForLeak() {
  if (leakSensorState == 1 && micros() - lastBeepTime >= (10 * 1000 * 1000)) {  // Alle 10 Sekunden
    beep("22222");                                                              // Hier die gewünschte Sequenz für den Ton
    log("WARNING LEAK", 22222, true);
    lastLeakBeepTime = micros();  // Aktualisieren Sie den Zeitpunkt des letzten Aufrufs
  }
}
void BeepForStandby() {
  if (motorState == MOTOR_STANDBY && micros() - lastStandbyBeepTime >= (1 * 60 * 1000000)) {
    beep("1");  // Hier die gewünschte Sequenz für den Ton
    log("still in standby", 1, true);
    lastStandbyBeepTime = micros();  // Aktualisieren Sie den Zeitpunkt des letzten Aufrufs
  }
}
void BlinkForLongStandby() {
  if (motorState == MOTOR_STANDBY && micros() - lastStandbyBlinkTime >= StandbyBlinkWarningtime && LED_State == 0) {
    blinkLED("111222111");  // Hier die gewünschte Sequenz für den Ton
    log("sos iam alone", 111222111, true);
    StandbyBlinkWarningtime = (StandbyBlinkDuration * 1000000);     //alle 30 sek
    lastStandbyBlinkTime = micros();  // Aktualisieren Sie den Zeitpunkt des letzten Aufrufs
  } else {
    StandbyBlinkWarningtime = (StandbyBlinkStart * 60 * 1000000);  //alle 3 minuten
  }
}

/*
idea to check all the click codes in one funtion. not working at the moment
*/
void checkButtonClicks() {
  leftButton.Update();
  rightButton.Update();

  if (leftButton.clicks == 1 && rightButton.clicks == 1) {
    Serial.println("1 Click - 1 Click");
  } else if (leftButton.clicks == 2 && rightButton.clicks == 2) {
    Serial.println("2 Clicks - 2 Clicks");
  } else if (leftButton.clicks == 3 && rightButton.clicks == 3) {
    Serial.println("3 Clicks - 3 Clicks");
  } else if (leftButton.clicks == 2 && rightButtonState == 0) {
    Serial.println("2 Clicks - Hold");
  } else if (leftButtonState == 0 && rightButton.clicks == 2) {
    Serial.println("Hold - 2 Clicks");
  } else if (leftButton.clicks == 1) {
    Serial.println("1 Click -");
  } else if (rightButton.clicks == 1) {
    Serial.println("- 1 Click");
  }
}

void setBar(int value, String hexColorOn, String hexColorOff) {
  // Konvertiere den Hex-Farbwert in RGB-Farbwerte für die eingeschaltete Farbe
  long numberOn = (long) strtol(&hexColorOn[1], NULL, 16);
  int redOn = numberOn >> 16;
  int greenOn = (numberOn >> 8) & 0xFF;
  int blueOn = numberOn & 0xFF;

  // Alle LEDs ausschalten
  strip.clear();

  // Berechnen, wie viele LEDs eingeschaltet werden sollen
  int NUM_LEDBAR_on = map(value, 0, 100, 0, LEDBAR_NUM);

  // Setze die LEDs entsprechend der berechneten Helligkeiten und der übergebenen Farben
  for (int i = 0; i < NUM_LEDBAR_on; i++) {
    if (i == NUM_LEDBAR_on - 1 && value % 10 != 0) { // Letzte LED mit 10% Helligkeit für krumme Werte
      int dimmed_color_r = redOn / 10;
      int dimmed_color_g = greenOn / 10;
      int dimmed_color_b = blueOn / 10;
      strip.setPixelColor(i, strip.Color(dimmed_color_r, dimmed_color_g, dimmed_color_b));
    } else {
      strip.setPixelColor(i, strip.Color(redOn, greenOn, blueOn));
    }
  }

  // Setze die LEDs für die ausgeschaltete Seite
  for (int i = NUM_LEDBAR_on; i < LEDBAR_NUM; i++) {
    // Konvertiere den Hex-Farbwert in RGB-Farbwerte für die ausgeschaltete Farbe
    long numberOff = (long) strtol(&hexColorOff[1], NULL, 16);
    int redOff = numberOff >> 16;
    int greenOff = (numberOff >> 8) & 0xFF;
    int blueOff = numberOff & 0xFF;
    strip.setPixelColor(i, strip.Color(redOff, greenOff, blueOff));
  }

  strip.show();  // LED-Streifen aktualisieren
}

void updateBatteryLevel(float voltage) {
  float singleCellVoltages[] = {4.2, 3.85, 3.75, 3.65, 3.55, 3.45, 3.35, 3.25, 3.15, 3.05, 2.5};
  int singleCellPercentages[] = {100, 90, 80, 70, 60, 50, 40, 30, 20, 10, 0};

  float measurements[batteryLevelMeasurements];
  for (int i = 0; i < batteryLevelMeasurements; i++) {
    measurements[i] = voltage; // Each measurement should be the same as the measured voltage
  }

  float sum = 0.0;
  for (int i = 0; i < batteryLevelMeasurements; i++) {
    sum += measurements[i];
  }
  float averageVoltage = sum / batteryLevelMeasurements;

  if (averageVoltage > singleCellVoltages[0] * CellsInSeries) {
    batteryLevel = 100;
  } else if (averageVoltage <= singleCellVoltages[sizeof(singleCellVoltages) / sizeof(singleCellVoltages[0]) - 1] * CellsInSeries) {
    batteryLevel = 0;
  } else {
    for (int i = 1; i < sizeof(singleCellVoltages) / sizeof(singleCellVoltages[0]); i++) {
      if (averageVoltage >= singleCellVoltages[i] * CellsInSeries) {
        float deltaV = singleCellVoltages[i - 1] * CellsInSeries - singleCellVoltages[i] * CellsInSeries;
        float deltaP = singleCellPercentages[i - 1] - singleCellPercentages[i];
        float slope = deltaP / deltaV;
        batteryLevel = singleCellPercentages[i] + slope * (singleCellVoltages[i] * CellsInSeries - averageVoltage);
        break;
      }
    }
  }
  
  // Ensure that the battery level is limited to the range [0, 100]
  batteryLevel = constrain(batteryLevel, 0, 100);
}

void saveData(const char* key, int value) {
  Preferences preferences;
  preferences.begin("myApp", false);  // Name der App anpassen
  preferences.putUInt(key, value);
  preferences.end();
}

int loadData(const char* key, int defaultValue) {
  Preferences preferences;
  preferences.begin("myApp", false);  // Name der App anpassen
  int value = preferences.getUInt(key, defaultValue);
  preferences.end();
  return value;
}

void loop() {
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
  BeepForLeak();
  BeepForStandby();
  BlinkForLongStandby();
  GetVESCValues();


//Serial.println(batteryLevel); // test battery level




  //Serial.println("up " + uptime_formatter::getUptime());
  //Sonst ist der controller zu schnell durch den loop
  /*
  implemented this delay because the code is not working correct if this is not used. dont know why
  */
  delay(1);
}
