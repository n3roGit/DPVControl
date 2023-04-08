#include <Servo.h>
#include <PinButton.h>


const int PIN_LEFT_BUTTON = 4;//19; //D2
const int PIN_RIGHT_BUTTON = 5; //D1
const int PIN_MOTOR = 0; //D3

const int MOTOR_OFF = 0;
const int MOTOR_ON = 1;
const int MOTOR_STANDBY = 2;

const int MOTOR_MAX_SPEED = 150;
const int MOTOR_MIN_SPEED = 30;

const int MOTOR_SPEED_CHANGE = MOTOR_MAX_SPEED*0.1;


int leftButtonState = 0;
int rightButtonState = 0;
int motorState = MOTOR_OFF;
int motorSpeed = 60;

PinButton leftButton(PIN_LEFT_BUTTON);
PinButton rightButton(PIN_RIGHT_BUTTON);

Servo servo;



void setup() {
  pinMode(PIN_LEFT_BUTTON, INPUT);
  pinMode(PIN_RIGHT_BUTTON, INPUT);

  Serial.begin(9600);

  servo.attach(PIN_MOTOR);
  servo.write(25); // needed for initializing the ESC
  delay(2000);

}

void updateMotorSpeed(){
  if (rightButton.isDoubleClick()){
    motorSpeed += MOTOR_SPEED_CHANGE;
    if (motorSpeed > MOTOR_MAX_SPEED){
      motorSpeed = MOTOR_MAX_SPEED;
    }
  }

  if (leftButton.isDoubleClick()){
    motorSpeed -= MOTOR_SPEED_CHANGE;
    if (motorSpeed < MOTOR_MIN_SPEED){
      motorSpeed = MOTOR_MIN_SPEED;
    }
  }

  Serial.print(" motorSpeed: ");
  Serial.print(motorSpeed);

}

void loop() {
  leftButton.update();
  rightButton.update();

  leftButtonState = digitalRead(PIN_LEFT_BUTTON);
  Serial.print("left: ");
  Serial.print(leftButtonState);

  rightButtonState = digitalRead(PIN_RIGHT_BUTTON);
  Serial.print(" right: ");
  Serial.print(rightButtonState);

  Serial.print(" millis: ");
  Serial.print(millis());

  updateMotorSpeed();


  if(leftButtonState == 1 || rightButtonState == 1){
    motorState = MOTOR_ON;
  }else{
    motorState = MOTOR_OFF;
  }
  Serial.print(" motorstate: ");
  Serial.print(motorState);

  if (motorState == MOTOR_STANDBY || motorState == MOTOR_OFF){
    //Motor is off
    servo.write(0);
  }else if (motorState == MOTOR_ON){
    servo.write(motorSpeed);
  }


  Serial.println();

}
