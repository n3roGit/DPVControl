#include <Servo.h>


const int leftButton = 4;//19; //D2
const int rightButton = 5; //D1
const int PIN_MOTOR = 0; //D3

const int MOTOR_OFF = 0;
const int MOTOR_ON = 1;
const int MOTOR_STANDBY = 2;


int leftButtonState = 0;
int rightButtonState = 0;
int previousRightButtonState = 0;
int rightClickCounter = 0;
int motorState = MOTOR_OFF;
int motorSpeed = 60;
Servo servo;



void setup() {
  pinMode(leftButton, INPUT);
  pinMode(rightButton, INPUT);

  Serial.begin(9600);

  servo.attach(PIN_MOTOR);
  servo.write(25); // needed for initializing the ESC
  delay(2000);

}

void loop() {
  leftButtonState = digitalRead(leftButton);
  Serial.print("left: ");
  Serial.print(leftButtonState);

  rightButtonState = digitalRead(rightButton);
  Serial.print(" right: ");
  Serial.print(rightButtonState);

  Serial.print(" millis: ");
  Serial.print(millis());


  if (rightButtonState == 1 && previousRightButtonState == 0){
    //Toggle detected
    rightClickCounter += 1;
    Serial.print(" rightClickCounter: ");
    Serial.print( rightClickCounter);
  }


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

  previousRightButtonState = rightButtonState;


  Serial.println();
  //delay(100);

}
