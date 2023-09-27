#include "ClickButton.h"

#include "DHTesp.h"

#include <HardwareSerial.h>

#include <VescUart.h>
VescUart UART;
#include "uptime_formatter.h"




/*
GPIO 16-33 kann man nutzen


*/

// https://wolles-elektronikkiste.de/en/programming-the-esp32-with-arduino-code

const int PIN_LEFT_BUTTON = 26;   //G26 OK
const int PIN_RIGHT_BUTTON = 27;  //G27 OK

const int PIN_LEAK_FRONT = 32;  //G32 OK
const int PIN_LEAK_BACK = 33;   //G3 OK

const int PIN_LED = 25;  //G25 OK

const int PIN_DHT = 14;  //G14 OK
DHTesp dhtSensor;

const int PIN_BEEP = 18;  //G18 OK

#define VESCRX 22  //OK
#define VESCTX 23  //OK




// Values for motorState
const int MOTOR_OFF = 0;
const int MOTOR_ON = 1;
const int MOTOR_STANDBY = 2;

//Constants
const int MOTOR_MAX_SPEED = 14000;
const int MOTOR_MIN_SPEED = 2000;
const int SPEED_UP_TIME_MS = 5000 * 1000;    //time we want to take to  speed the motor from 0 to  full power.
const int SPEED_DOWN_TIME_MS = 1000 * 1000;  //time we want to take to  speed the motor from full power to 0.
const int SPEED_STEPS = 10;                  //Number speed steps
const int MOTOR_SPEED_CHANGE = MOTOR_MAX_SPEED / SPEED_STEPS;
const int STANDBY_DELAY_MS = 45 /*s*/ * 1000 * 1000;  // Time until the motor goes into standby.
const bool EnableDebugLog = false;                    //Enable/Disable Serial Log
const float LED_Energy_Limiter = 0.8;
const int MotorButtonDelay = 500 * 1000;  //time befor button press the motor starts




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
unsigned long buttonPressStartTime = 0;
unsigned long lastLeakBeepTime = 0;


//IO
ClickButton leftButton(PIN_LEFT_BUTTON, HIGH, CLICKBTN_PULLUP);
ClickButton rightButton(PIN_RIGHT_BUTTON, HIGH, CLICKBTN_PULLUP);



void setup() {
  pinMode(PIN_LEFT_BUTTON, INPUT);
  pinMode(PIN_RIGHT_BUTTON, INPUT);
  pinMode(PIN_LEAK_FRONT, INPUT_PULLUP);  // Aktiviere den internen Pull-Up-Widerstand für den Front-Leak-Pin
  pinMode(PIN_LEAK_BACK, INPUT_PULLUP);   // Aktiviere den internen Pull-Up-Widerstand für den Back-Leak-Pin
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BEEP, OUTPUT);

  leftButton.debounceTime = 40;     //20
  leftButton.multiclickTime = 500;  //500
  leftButton.longClickTime = 1000;
  rightButton.debounceTime = 40;     //20
  rightButton.multiclickTime = 500;  //500
  rightButton.longClickTime = 1000;

  Serial.begin(115200);

  //BEEP Initial
  Serial.println("Booting started...!");
  beep("1");

  //Setup DHT22
  dhtSensor.setup(PIN_DHT, DHTesp::DHT22);
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  Serial.println("Temp: " + String(data.temperature, 2) + "°C");
  Serial.println("Humidity: " + String(data.humidity, 1) + "%");
  Serial.println("---");

  //VESC UART
  Serial1.begin(115200, SERIAL_8N1, VESCRX, VESCTX);
  while (!Serial1) { ; }
  delay(500);
  UART.setSerialPort(&Serial1);
  delay(500);
  if (UART.getVescValues()) {
    Serial.println("Verbindung zu VESC erfolgreich.");
  } else {
    Serial.println("Fehler beim Herstellen der Verbindung zu VESC.");
  }

  // Booting finished
  Serial.println("Booting finished!");
  //BEEP end
  beep("222");
}

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



void updateSpeedSetting() {
  if (motorState != MOTOR_STANDBY) {
    if (rightButton.clicks == -2) {
      log("rightButton.clicks", rightButton.clicks, true);
      speedSetting += MOTOR_SPEED_CHANGE;
      if (speedSetting > MOTOR_MAX_SPEED) {
        speedSetting = MOTOR_MAX_SPEED;
      }
      log("speedSetting", speedSetting, true);
    }

    if (leftButton.clicks == -2) {
      log("leftButton.clicks", leftButton.clicks, true);
      speedSetting -= MOTOR_SPEED_CHANGE;
      if (speedSetting < MOTOR_MIN_SPEED) {
        speedSetting = MOTOR_MIN_SPEED;
      }
      log("speedSetting", speedSetting, true);
    }
  }
}

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
      beep("22");
    }
    if (leftButtonState || rightButtonState) {
      //While not in standby, any button click updates the standby counter.
      lastActionTime = micros();
      log("update lastActionTime", lastActionTime, EnableDebugLog);
    }
  }
}


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
    //Motor is off
    targetMotorSpeed = 0;
  } else if (motorState == MOTOR_ON) {
    targetMotorSpeed = speedSetting;
  }
  //Serial.print(" targetMotorSpeed: ");
  //Serial.print(targetMotorSpeed);

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
    
    /*
    // Hinzugefügte Logik zur Überprüfung der Geschwindigkeit und LED_State
    if (speedSetting > int(MOTOR_MAX_SPEED * LED_Energy_Limiter) && LED_State != 0 && LED_State >= 3) {
      LED_State = 1;
      LED_State_Last = 1;
    } else {
      LED_State_Last = LED_State;
    }
    */
  }
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
  analogWrite(PIN_LED, brightness);  // LED-PIN, Brightness 0-255
  log("LED_State", LED_State, true);
}





void beep(const String& sequence) {
  for (char c : sequence) {
    int toneDuration = (c == '1') ? 200 : 600;
    digitalWrite(PIN_BEEP, HIGH);
    float startMicros = micros();
    while (micros() - startMicros < toneDuration * 1000) {
      // Warten, bis die gewünschte Dauer erreicht ist
    }
    digitalWrite(PIN_BEEP, LOW);
    lastBeepTime = micros();
    while (micros() - lastBeepTime < 400000) {
      // Pause zwischen den Tönen
    }
  }
}




void GetESCValues() {
  if (UART.getVescValues()) {
    Serial.print("RPM: ");
    Serial.println(UART.data.rpm);
    Serial.print("inpVoltage: ");
    Serial.println(UART.data.inpVoltage);
    Serial.print("ampHours: ");
    Serial.println(UART.data.ampHours);
    Serial.print("tachometerAbs: ");
    Serial.println(UART.data.tachometerAbs);
  } else {
    Serial.println("Failed to get data!");
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
  if (leakSensorState == 1 && micros() - lastBeepTime >= 10000000) {  // Alle 10 Sekunden
    beep("22222");                                                    // Hier die gewünschte Sequenz für den Ton
    log("WARNING", 22222, true);
    lastLeakBeepTime = micros();  // Aktualisieren Sie den Zeitpunkt des letzten Aufrufs
  }
}



void loop() {
  leftButton.Update();
  rightButton.Update();

  leftButtonState = digitalRead(PIN_LEFT_BUTTON);
  rightButtonState = digitalRead(PIN_RIGHT_BUTTON);

  log("rightButtonState", rightButtonState, EnableDebugLog);
  log("leftButtonState", leftButtonState, EnableDebugLog);

  updateSpeedSetting();
  controlStandby();
  controlMotor();
  controlLED();
  checkForLeak();
  BeepForLeak();
  //Serial.println("up " + uptime_formatter::getUptime());
}
