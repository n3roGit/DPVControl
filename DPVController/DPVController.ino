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
<!DOCTYPE HTML><html>
<head>
  <title>ESP Web Server</title>
</head>
<body>
<h1>DPVControl&nbsp; &nbsp; &nbsp; &nbsp; v0.1</h1>
<h2>Wifi</h2>
<p>SSID: Aquazepp</p>
<p>Password: Aquazepp</p>
<h2>Akku</h2>
<p>Akkuspannung: x Volt</p>
<p>Ladezustand: x %</p>
<h2>Sensoren</h2>
<p>Wassersensor:&nbsp;</p>
<p>Uptime:&nbsp;</p>
<p>Schalter Rechts: AUS/AN</p>
<p>Schalter Links: AUS/AN</p>
<p>&nbsp;</p>
<h2>Log:</h2>
<p>xxxx</p>
<p>xxxx</p>
<p>xxxx</p>
<p>xxxx</p>
<p>&nbsp;</p>
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
  Serial.print("AP IP address: ");
  Serial.println(IP);
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
  }
}

void updateSpeedSetting(){
  if (motorState != MOTOR_STANDBY){
    log("rightButton.clicks", rightButton.clicks, true);
    if (rightButton.clicks == -2){
      speedSetting += MOTOR_SPEED_CHANGE;
      if (speedSetting > MOTOR_MAX_SPEED){
        speedSetting = MOTOR_MAX_SPEED;
      }
    }

    if (leftButton.clicks == -2){
      speedSetting -= MOTOR_SPEED_CHANGE;
      if (speedSetting < MOTOR_MIN_SPEED){
        speedSetting = MOTOR_MIN_SPEED;
      }
    }

    log("speedSetting", speedSetting, true);
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
      log("update lastActionTime", lastActionTime, true);
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
  log("motorstate", motorState, true);

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

  Serial.println();

}
