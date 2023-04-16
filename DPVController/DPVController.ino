#include <Servo.h>
#include "ClickButton.h"


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
const int MOTOR_MIN_SPEED = 25;
const int SPEED_UP_TIME_MS = 4000; //time we want to take to  speed the motor from 0 to  full power.
const int SPEED_DOWN_TIME_MS = 400; //time we want to take to  speed the motor from full power to 0.
const int SPEED_STEPS = 5; //Number speed steps
const int MOTOR_SPEED_CHANGE = MOTOR_MAX_SPEED/SPEED_STEPS;


//Variables
int leftButtonState = 0;
int rightButtonState = 0;
int leakSensorState = 0;
int motorState = MOTOR_OFF;
int currentMotorSpeed = 0; //Speed the motor is currently running at
int currentMotorTime = 0; //Time in MS when we last changed the currentMotorSpeed
int speedSetting = MOTOR_MIN_SPEED; //The current speed setting. stays the same, even if motor is turned off. 
int targetMotorSpeed = 0; //The desired motor speed


//IO
ClickButton leftButton(PIN_LEFT_BUTTON);
ClickButton rightButton(PIN_RIGHT_BUTTON);
ClickButton LeakSensor(PIN_LEAK);
Servo servo;

void setup() {
  pinMode(PIN_LEFT_BUTTON, INPUT);
  pinMode(PIN_RIGHT_BUTTON, INPUT);
  pinMode(PIN_LEAK, INPUT);

  Serial.begin(9600);

  servo.attach(PIN_MOTOR);
  servo.write(25); // needed for initializing the ESC
  delay(2000);

}

void updateSpeedSetting(){
  if (rightButton.clicks == 2){
    speedSetting += MOTOR_SPEED_CHANGE;
    if (speedSetting > MOTOR_MAX_SPEED){
      speedSetting = MOTOR_MAX_SPEED;
    }
  }

  if (leftButton.clicks == 2){
    speedSetting -= MOTOR_SPEED_CHANGE;
    if (speedSetting < MOTOR_MIN_SPEED){
      speedSetting = MOTOR_MIN_SPEED;
    }
  }

  Serial.print(" speedSetting: ");
  Serial.print(speedSetting);

}

void controlMotor(){
  if((leftButtonState == 1 || rightButtonState == 1) && leakSensorState == 0){
    motorState = MOTOR_ON;
  }else{
    motorState = MOTOR_OFF;
  }
  Serial.print(" motorstate: ");
  Serial.print(motorState);

  if (motorState == MOTOR_STANDBY || motorState == MOTOR_OFF){
    //Motor is off
    targetMotorSpeed = 0;
  }else if (motorState == MOTOR_ON){
    targetMotorSpeed = speedSetting;
  }
  Serial.print(" targetMotorSpeed: ");
  Serial.print(targetMotorSpeed);

  setSoftMotorSpeed();
}

/**
* Slowly changes the motor speed to targetMotorSpeed.
*
**/
void setSoftMotorSpeed(){

  int timePassedSinceLastChange = millis() - currentMotorTime;
  bool speedUp = currentMotorSpeed < targetMotorSpeed;
  Serial.print(" speedUp: ");
  Serial.print(speedUp);
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
  Serial.print(" currentMotorSpeed: ");
  Serial.print(currentMotorSpeed);
  currentMotorTime = millis();
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

  leakSensorState = digitalRead(PIN_LEAK);
  Serial.print(" leak: ");
  Serial.print(leakSensorState);

  Serial.print(" millis: ");
  Serial.print(millis());

  updateSpeedSetting();
  controlMotor();

  Serial.println();

}
