#include <Servo.h>
#include <PinButton.h>


const int PIN_LEFT_BUTTON = 4; //D2
const int PIN_RIGHT_BUTTON = 5; //D1

const int PIN_LEAK = 16; //D0

const int PIN_MOTOR = 0; //D3

const int MOTOR_OFF = 0;
const int MOTOR_ON = 1;
const int MOTOR_STANDBY = 2;

const int MOTOR_MAX_SPEED = 160;
const int MOTOR_MIN_SPEED = 5;
const int SPEED_UP_TIME_MS = 4000;

const int SPEED_STEPS = 0.05;
const int MOTOR_SPEED_CHANGE = MOTOR_MAX_SPEED*0.05;


int leftButtonState = 0;
int rightButtonState = 0;
int leakSensorState = 0;
int motorState = MOTOR_OFF;
int currentMotorSpeed = 0; //Speed the motor is currently running at
int targetMotorSpeed = MOTOR_MIN_SPEED;
int motorStartTime = 0;

PinButton leftButton(PIN_LEFT_BUTTON);
PinButton rightButton(PIN_RIGHT_BUTTON);
PinButton LeakSensor(PIN_LEAK);


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

void updateTargetMotorSpeed(){
  if (rightButton.isDoubleClick()){
    targetMotorSpeed += MOTOR_SPEED_CHANGE;
    if (targetMotorSpeed > MOTOR_MAX_SPEED){
      targetMotorSpeed = MOTOR_MAX_SPEED;
    }
  }

  if (leftButton.isDoubleClick()){
    targetMotorSpeed -= MOTOR_SPEED_CHANGE;
    if (targetMotorSpeed < MOTOR_MIN_SPEED){
      targetMotorSpeed = MOTOR_MIN_SPEED;
    }
  }

  Serial.print(" targetMotorSpeed: ");
  Serial.print(targetMotorSpeed);

}

void controlMotor(){
  if((leftButtonState == 1 || rightButtonState == 1) && leakSensorState == 0){
    //motor was just turned on
    if (motorState != MOTOR_ON){
      motorStartTime = millis();
    }
    motorState = MOTOR_ON;
  }else{
    motorState = MOTOR_OFF;
  }
  Serial.print(" motorstate: ");
  Serial.print(motorState);

  if (motorState == MOTOR_STANDBY || motorState == MOTOR_OFF){
    //Motor is off
    servo.write(0);
    currentMotorSpeed = 0;
    targetMotorSpeed = 0;
  }else if (motorState == MOTOR_ON){

    int diff = millis() - motorStartTime;
    int end_speed_up = SPEED_UP_TIME_MS * targetMotorSpeed / MOTOR_MAX_SPEED;
    if (diff >= end_speed_up){
      currentMotorSpeed = targetMotorSpeed; 
    }else{
      currentMotorSpeed = targetMotorSpeed * diff / end_speed_up;
    }
    servo.write(currentMotorSpeed);
  }
  Serial.print(" currentMotorSpeed: ");
  Serial.print(currentMotorSpeed);
}



void loop() {
  leftButton.update();
  rightButton.update();
  LeakSensor.update();

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

  updateTargetMotorSpeed();
  controlMotor();

  Serial.println();

}
