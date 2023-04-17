#include <Servo.h>
#include "ClickButton.h"

#include <ESP8266WiFi.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

// https://randomnerdtutorials.com/esp8266-nodemcu-access-point-ap-web-server/



const char* ssid      = "Aquazepp";     // Not needed as this program includes the WiFi manager, see the instructions later
const char* password  = "Aquazepp"; // Not needed as this program includes the WiFi manager, see the instructions later

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
                <td><button class="button">&Auml;ndern</button></td>
            </tr>
            <tr>
                <td>Password:</td>
                <td><input type="text" value="Aquazepp" /></td>
                <td><button class="button">&Auml;ndern</button></td>
            </tr>
        </table>
        <h2>Akku</h2>
        <table>
            <tr>
                <td>Akkuspannung:</td>
                <td><input type="text" value="52 Volt" readonly /></td>
            </tr>
            <tr>
                <td>Ladezustand:</td>
                <td><input type="text" value="100 %" readonly /></td>
            </tr>
        </table>
        <h2>Konfiguration</h2>
        <table>
            <tr>
                <td>Min Speed:</td>
                <td><input type="text" value="30" /></td>
                <td><button class="button">&Auml;ndern</button></td>
            </tr>
            <tr>
                <td>Max Speed:</td>
                <td><input type="text" value="160" /></td>
                <td><button class="button">&Auml;ndern</button></td>
            </tr>
            <tr>
                <td>SpinUpTime:</td>
                <td><input type="text" value="4000 ms" /></td>
                <td><button class="button">&Auml;ndern</button></td>
            </tr>
            <tr>
                <td>SpinDownTime:</td>
                <td><input type="text" value="400 ms" /></td>
                <td><button class="button">&Auml;ndern</button></td>
            </tr>
            <tr>
                <td>Speed Steps:</td>
                <td><input type="text" id="speedSteps" value="5" /></td>
                <td><button class="button" onclick="updateValue('speedSteps')">&Auml;ndern</button></td>
            </tr>
            <tr>
                <td>StandBy Time:</td>
                <td><input type="text" id="standbyTime" value="45" /></td>
                <td><button class="button" onclick="updateValue('standbyTime')">&Auml;ndern</button></td>
            </tr>
        </table>

        <h2>Daten</h2>
        <p>Wassersensor: <span id="waterSensorOutput"></span> <button onclick="acknowledge()">ACK</button></p>
        <p>Uptime: <span id="uptimeOutput"></span></p>
        <p>Schalter Rechts: <span id="rightSwitchOutput"></span></p>
        <p>Schalter Links: <span id="leftSwitchOutput"></span></p>
        <p>Speed Preset: <span id="speedPresetOutput"></span></p>
        <p>Aktueller Speed: <span id="currentSpeedOutput"></span></p>
        <p>ESC Ampere: <span id="escAmpereOutput"></span></p>
        <p>ESC Drehzahl: <span id="escRPMOutput"></span></p>
        <p>ESC Temperatur: <span id="escTempOutput"></span></p>

        <p>&nbsp;</p>
        <h2>Log:</h2>
        <div style="border: 1px solid black; width: 600px; height: 200px; overflow: auto;">
            <div id="logOutput"></div>
        </div>
        <p>&nbsp;</p>
        <button onclick="restart()">Neustart</button>
        <h4>Version 0.1</h4>
    </body>
</html>


)rawliteral";

// PIN constants
const int PIN_LEFT_BUTTON = 4; //D2
const int PIN_RIGHT_BUTTON = 5; //D1
const int PIN_LEAK = 16; //D0
const int PIN_MOTOR = 0; //D3

// Values for motorState
const int MOTOR_OFF = 0;
const int MOTOR_ON = 1;
const int MOTOR_STANDBY = 2;

//Constants
const int MOTOR_MAX_SPEED = 160;
const int MOTOR_MIN_SPEED = 30;
const int SPEED_UP_TIME_MS = 4000; //time we want to take to  speed the motor from 0 to  full power.
const int SPEED_DOWN_TIME_MS = 400; //time we want to take to  speed the motor from full power to 0.
const int SPEED_STEPS = 5; //Number speed steps
const int MOTOR_SPEED_CHANGE = MOTOR_MAX_SPEED/SPEED_STEPS;
const int STANDBY_DELAY_MS = 45/*s*/ * 1000; // Time until the motor goes into standby. 
const bool EnableLog = false; //Enable/Disable Serial Log

//Variables
int leftButtonState = 0;
int rightButtonState = 0;
int leakSensorState = 0;
int motorState = MOTOR_STANDBY;
int currentMotorSpeed = 0; //Speed the motor is currently running at
int currentMotorTime = 0; //Time in MS when we last changed the currentMotorSpeed
int speedSetting = MOTOR_MIN_SPEED; //The current speed setting. stays the same, even if motor is turned off. 
int targetMotorSpeed = 0; //The desired motor speed
int lastActionTime = 0;



//IO
ClickButton leftButton(PIN_LEFT_BUTTON, LOW, CLICKBTN_PULLUP);
ClickButton rightButton(PIN_RIGHT_BUTTON, LOW, CLICKBTN_PULLUP);
ClickButton LeakSensor(PIN_LEAK);
Servo servo;

void setup() {
  pinMode(PIN_LEFT_BUTTON, INPUT);
  pinMode(PIN_RIGHT_BUTTON, INPUT);
  pinMode(PIN_LEAK, INPUT);

  leftButton.debounceTime   = 20;   
  leftButton.multiclickTime = 500;  
  leftButton.longClickTime  = 1000;
  rightButton.debounceTime   = 20;   
  rightButton.multiclickTime = 500;  
  rightButton.longClickTime  = 1000;

  Serial.begin(9600);

  servo.attach(PIN_MOTOR);
  servo.write(25); // needed for initializing the ESC
  delay(2000);

  
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  log("AP IP address:", IP, true);
  Serial.println(WiFi.localIP());
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  // Start server
  server.begin();
}

void log(const char * label, int value, boolean doLog){
  if (doLog){
    Serial.print(" ");
    Serial.print(label);
    Serial.print(": ");
    Serial.print(value);
    Serial.println();
  }
}

void updateSpeedSetting(){
  if (motorState != MOTOR_STANDBY){
    if (rightButton.clicks == -2){
      log("rightButton.clicks", rightButton.clicks, true);
      speedSetting += MOTOR_SPEED_CHANGE;
      if (speedSetting > MOTOR_MAX_SPEED){
        speedSetting = MOTOR_MAX_SPEED;
      }
    }

    if (leftButton.clicks == -2){
      log("leftButton.clicks", leftButton.clicks, true);
      speedSetting -= MOTOR_SPEED_CHANGE;
      if (speedSetting < MOTOR_MIN_SPEED){
        speedSetting = MOTOR_MIN_SPEED;
      }
    }

    log("speedSetting", speedSetting, EnableLog);
  }
}

void controlStandby(){
  if (motorState == MOTOR_STANDBY){
    //Wake up from Standup
    if (leftButton.clicks == -2 || rightButton.clicks == -2){
      motorState = MOTOR_OFF;
      log("leaving standby", 1, true);
      lastActionTime = millis();
    }
  }else{
    if (lastActionTime + STANDBY_DELAY_MS < millis()){
      //Go into standby
      log("going to standby", millis(), true);
      motorState = MOTOR_STANDBY;
    }
    if(leftButtonState || rightButtonState){
      //While not in standby, any button click updates the standby counter.
      lastActionTime = millis();
      log("update lastActionTime", lastActionTime, EnableLog);
    }
  }
  
}

void controlMotor(){
  if (motorState != MOTOR_STANDBY){
    if((leftButtonState == 1 || rightButtonState == 1) && leakSensorState == 0){
      motorState = MOTOR_ON;
    }else{
      motorState = MOTOR_OFF;
    }
  }
  log("motorstate", motorState, EnableLog);

  if (motorState == MOTOR_STANDBY || motorState == MOTOR_OFF){
    //Motor is off
    targetMotorSpeed = 0;
  }else if (motorState == MOTOR_ON){
    targetMotorSpeed = speedSetting;
  }
  //Serial.print(" targetMotorSpeed: ");
  //Serial.print(targetMotorSpeed);

  setSoftMotorSpeed();
}

/**
* Slowly changes the motor speed to targetMotorSpeed.
*
**/
void setSoftMotorSpeed(){

  int timePassedSinceLastChange = millis() - currentMotorTime;
  bool speedUp = currentMotorSpeed < targetMotorSpeed;
  //Serial.print(" speedUp: ");
  //Serial.print(speedUp);
  if (speedUp){
    int maxChange = timePassedSinceLastChange * MOTOR_MAX_SPEED  / SPEED_UP_TIME_MS ;
    currentMotorSpeed += maxChange;
    currentMotorSpeed = min(currentMotorSpeed, targetMotorSpeed);
  }else{
    int maxChange = timePassedSinceLastChange * MOTOR_MAX_SPEED  / SPEED_DOWN_TIME_MS ;
    currentMotorSpeed -= maxChange;
    currentMotorSpeed = max(currentMotorSpeed, targetMotorSpeed);
  }
  servo.write(currentMotorSpeed);
  //Serial.print(" currentMotorSpeed: ");
  //Serial.print(currentMotorSpeed);
  currentMotorTime = millis();
}


void loop() {
  leftButton.Update();
  rightButton.Update();
  LeakSensor.Update();

  leftButtonState = digitalRead(PIN_LEFT_BUTTON);
  //Serial.print("left: ");
  //Serial.print(leftButtonState);

  rightButtonState = digitalRead(PIN_RIGHT_BUTTON);
  //Serial.print(" right: ");
  //Serial.print(rightButtonState);

  leakSensorState = digitalRead(PIN_LEAK);
  //Serial.print(" leak: ");
  //Serial.print(leakSensorState);

  //Serial.print(" millis: ");
  //Serial.print(millis());

  updateSpeedSetting();
  controlStandby();
  controlMotor();


}
