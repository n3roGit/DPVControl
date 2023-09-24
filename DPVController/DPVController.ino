#include "ClickButton.h"

//#include <ESP8266WiFi.h>
//#include <Hash.h>
//#include <ESPAsyncTCP.h>
//#include <ESPAsyncWebServer.h>



#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#include <HardwareSerial.h>
#include <VescUart.h>
VescUart UART;
#define RXD1 16
#define TXD1 17


// https://randomnerdtutorials.com/esp8266-nodemcu-access-point-ap-web-server/

#include "uptime_formatter.h"

const char* ssid = "Aquazepp";      // Not needed as this program includes the WiFi manager, see the instructions later
const char* password = "Aquazepp";  // Not needed as this program includes the WiFi manager, see the instructions later

/*
AsyncWebServer server(80);
const char index_html[] PROGMEM = R"rawliteral(

<!DOCTYPE html>
<html>
    <head>
        <title>DPVControl</title>
        <meta name="viewport" content="width=device-width, initial-scale=1" />
        <style>
            body {
                font-family: Arial, sans-serif;
                margin: 0;
                padding: 0;
            }
            h2 {
                margin-top: 0.5em;
            }

            table {
                border-collapse: collapse;
                width: 100%;
            }

            th,
            td {
                text-align: left;
                padding: 8px;
                border-bottom: 1px solid #ddd;
            }

            tr:hover {
                background-color: #f5f5f5;
            }

            .button {
                background-color: #4caf50;
                border: none;
                color: white;
                padding: 10px 20px;
                text-align: center;
                text-decoration: none;
                display: inline-block;
                font-size: 16px;
                margin: 4px 2px;
                cursor: pointer;
            }

            .button:hover {
                background-color: #3e8e41;
            }

            #log {
                height: 200px;
                overflow-y: scroll;
                white-space: pre-wrap;
                border: 1px solid #ddd;
                padding: 8px;
            }

            #log p:last-child {
                margin-bottom: 0;
            }
        </style>
    </head>
    <body>
        <h2>Wifi</h2>
        <table>
            <tr>
                <td>SSID:</td>
                <td><input type="text" value="Aquazepp" /></td>
                <td><button class="button">Change</button></td>
            </tr>
            <tr>
                <td>Password:</td>
                <td><input type="text" value="Aquazepp" /></td>
                <td><button class="button">Change</button></td>
            </tr>
        </table>
        <h2>Battery</h2>
        <table>
            <tr>
                <td>Battery Voltage:</td>
                <td><input type="text" value="52 Volts" readonly /></td>
            </tr>
            <tr>
                <td>Charge Level:</td>
                <td><input type="text" value="100 %" readonly /></td>
            </tr>
        </table>
        <h2>Configuration</h2>
        <table>
            <tr>
                <td>Min Speed:</td>
                <td><input type="text" value="30" /></td>
                <td><button class="button">Change</button></td>
            </tr>
            <tr>
                <td>Max Speed:</td>
                <td><input type="text" value="160" /></td>
                <td><button class="button">Change</button></td>
            </tr>
            <tr>
                <td>SpinUp Time:</td>
                <td><input type="text" value="4000 ms" /></td>
                <td><button class="button">Change</button></td>
            </tr>
            <tr>
                <td>SpinDown Time:</td>
                <td><input type="text" value="400 ms" /></td>
                <td><button class="button">Change</button></td>
            </tr>
            <tr>
                <td>Speed Steps:</td>
                <td><input type="text" id="speedSteps" value="5" /></td>
                <td><button class="button" onclick="updateValue('speedSteps')">Change</button></td>
            </tr>
            <tr>
                <td>StandBy Time:</td>
                <td><input type="text" id="standbyTime" value="45" /></td>
                <td><button class="button" onclick="updateValue('standbyTime')">Change</button></td>
            </tr>
        </table>

        <h2>Data</h2>
        <p>Water Sensor: <span id="waterSensorOutput"></span> <button onclick="acknowledge()">ACK</button></p>
        <p>Uptime: <span id="uptimeOutput"></span></p>
        <p>Right Switch: <span id="rightSwitchOutput"></span></p>
        <p>Left Switch: <span id="leftSwitchOutput"></span></p>
        <p>Speed Preset: <span id="speedPresetOutput"></span></p>
        <p>Current Speed: <span id="currentSpeedOutput"></span></p>
        <p>ESC Ampere: <span id="escAmpereOutput"></span></p>
        <p>ESC RPM: <span id="escRPMOutput"></span></p>
        <p>ESC Temperature: <span id="escTempOutput"></span></p>

        <p>&nbsp;</p>
        <h2>Log:</h2>
        <div style="border: 1px solid black; width: 600px; height: 200px; overflow: auto;">
            <div id="logOutput"></div>
        </div>
        <p>&nbsp;</p>
        <button onclick="restart()">Restart</button>
        <h4>Version 0.1</h4>
    </body>
</html>
)rawliteral";
*/

/*
PIN constants
34-39 only input without pulldown/up


GPIO0: Boot-Modus (boot mode), oft für Flashen verwendet.
GPIO2: Allgemeiner GPIO-Pin.
GPIO4, GPIO5, GPIO12, GPIO13: Allgemeine GPIO-Pins, geeignet für Taster als Eingänge.
GPIO14: Eingang für den DHT22-Sensor (Temperatur und Luftfeuchtigkeit).
GPIO15: Allgemeiner GPIO-Pin.
GPIO16: Allgemeiner GPIO-Pin, kann für serielle Kommunikation (RX2) verwendet werden.
GPIO17: Allgemeiner GPIO-Pin, kann für serielle Kommunikation (TX2) verwendet werden.
GPIO18: Ausgang für das Schalten eines Beepers oder ähnlicher Geräte.
GPIO2, GPIO4, GPIO5, GPIO12, GPIO13, GPIO14, GPIO15, GPIO16, GPIO17, GPIO18: PWM-fähige Pins für Pulsweitenmodulation (PWM).
GPIO19, GPIO21, GPIO22, GPIO23: Allgemeine GPIO-Pins.
GPIO25, GPIO26, GPIO27, GPIO32, GPIO33, GPIO34, GPIO35: Weitere allgemeine GPIO-Pins.


*/

// https://wolles-elektronikkiste.de/en/programming-the-esp32-with-arduino-code

const int PIN_LEFT_BUTTON = 26;   //G26 OK
const int PIN_RIGHT_BUTTON = 27;  //G27 OK

const int PIN_LEAK_FRONT = 32;         //G12
const int PIN_LEAK_BACK = 33;         //G13

const int PIN_LED = 25;           //G25 OK

const int PIN_DHT = 14;           //G14
const int PIN_BEEP = 18;           //G18 OK





#define DHTTYPE    DHT22
DHT_Unified dht(PIN_DHT, DHTTYPE);

// Values for motorState
const int MOTOR_OFF = 0;
const int MOTOR_ON = 1;
const int MOTOR_STANDBY = 2;

//Constants
const int MOTOR_MAX_SPEED = 14000;
const int MOTOR_MIN_SPEED = 2000;
const int SPEED_UP_TIME_MS = 80000;   //time we want to take to  speed the motor from 0 to  full power.
const int SPEED_DOWN_TIME_MS = 1000;  //time we want to take to  speed the motor from full power to 0.
const int SPEED_STEPS = 10;           //Number speed steps
const float MOTOR_SPEED_CHANGE = MOTOR_MAX_SPEED / SPEED_STEPS;
const int STANDBY_DELAY_MS = 45 /*s*/ * 1000 * 1000;  // Time until the motor goes into standby.
const bool EnableDebugLog = false;                    //Enable/Disable Serial Log
const int LED_Energy_Limiter = 0.8;

//Variables
int leftButtonState = 0;
int rightButtonState = 0;
int leakSensorState = 0;
int motorState = MOTOR_STANDBY;
int currentMotorSpeed = 0;           //Speed the motor is currently running at
float currentMotorTime = 0;          //Time in MS when we last changed the currentMotorSpeed
int speedSetting = MOTOR_MIN_SPEED;  //The current speed setting. stays the same, even if motor is turned off.
int targetMotorSpeed = 0;            //The desired motor speed
int lastActionTime = 0;
int LED_State = 0;
int LED_State_Last = 0;



//IO
ClickButton leftButton(PIN_LEFT_BUTTON, HIGH, CLICKBTN_PULLUP);
ClickButton rightButton(PIN_RIGHT_BUTTON, HIGH, CLICKBTN_PULLUP);
ClickButton LeakSensor(PIN_LEAK_FRONT);
//ClickButton LeakSensor(PIN_LEAK_BACK);


void setup() {
  pinMode(PIN_LEFT_BUTTON, INPUT);
  pinMode(PIN_RIGHT_BUTTON, INPUT);
  pinMode(PIN_LEAK_FRONT, INPUT);
  //pinMode(PIN_LEAK_BACK, INPUT);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BEEP, OUTPUT);

  leftButton.debounceTime = 20;
  leftButton.multiclickTime = 500;
  leftButton.longClickTime = 1000;
  rightButton.debounceTime = 20;
  rightButton.multiclickTime = 500;
  rightButton.longClickTime = 1000;

  Serial.begin(115200);

  //BEEP Initial
  Serial.println("Booting started...!");
  beep("1");

/*
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  log("AP IP address:", IP, true);
  Serial.println(WiFi.localIP());
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html", index_html);
  });

  // Start server
  server.begin();
*/
  //DHT Initial
  dht.begin();
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  //delayMS = sensor.min_delay / 1000;



  //VESC UART
  Serial1.begin(115200, SERIAL_8N1, RXD1, TXD1);
  while (!Serial1) {;}
  
  UART.setSerialPort(&Serial1);

  if (UART.getVescValues()) 
  {
    Serial.println("Verbindung zu VESC erfolgreich.");
  }
  else
  {
    Serial.println("Fehler beim Herstellen der Verbindung zu VESC.");
  }

Serial.println("Booting finished!");
  //BEEP Initial
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

void controlMotor() {
  if (motorState != MOTOR_STANDBY) {
    if ((leftButtonState == 0 || rightButtonState == 0) && leakSensorState == 0) {
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

    setLEDState(LED_State);
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
  analogWrite(PIN_LED, brightness); // LED-PIN, Brightness 0-255
  log("LED_State", LED_State, true);
}

void beep(const String& sequence) {
  for (char c : sequence) {
    int toneDuration = (c == '1') ? 200 : 600; // Dauer des Tons: 200 ms für kurz (1), 600 ms für lang (2)
    digitalWrite(PIN_BEEP, HIGH);
    delay(toneDuration);
    digitalWrite(PIN_BEEP, LOW);
    delay(400); // Pause zwischen den Tönen (in Millisekunden)
  }
}

void loop() {
  leftButton.Update();
  rightButton.Update();
  LeakSensor.Update();

  leftButtonState = digitalRead(PIN_LEFT_BUTTON);
  Serial.print("left: ");
 Serial.print(leftButtonState);

  rightButtonState = digitalRead(PIN_RIGHT_BUTTON);
  Serial.print(" right: ");
  Serial.print(rightButtonState);

  leakSensorState = digitalRead(PIN_LEAK_FRONT);
  Serial.print(" leak: ");
  Serial.print(leakSensorState);

  //Serial.print(" micros: ");
  //Serial.print(micros());

  updateSpeedSetting();
  controlStandby();
  controlMotor();
  controlLED();

  //Serial.println("up " + uptime_formatter::getUptime());
}
