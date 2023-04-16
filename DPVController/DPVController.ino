#include <Servo.h>
#include <PinButton.h>

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
const int MOTOR_MIN_SPEED = 5;
const int SPEED_UP_TIME_MS = 4000; //time we want to take to  speed the motor from 0 to  full power.
const int SPEED_STEPS = 20; //Number speed steps
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

void updateSpeedSetting(){
  if (rightButton.isDoubleClick()){
    speedSetting += MOTOR_SPEED_CHANGE;
    if (speedSetting > MOTOR_MAX_SPEED){
      speedSetting = MOTOR_MAX_SPEED;
    }
  }

  if (leftButton.isDoubleClick()){
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

  }
  Serial.print(" currentMotorSpeed: ");
  Serial.print(currentMotorSpeed);
}


void setSoftMotorSpeed(){

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

  updateSpeedSetting();
  controlMotor();

  Serial.println();

}
