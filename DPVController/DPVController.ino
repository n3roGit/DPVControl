#include <Servo.h>
#include <ClickButton.h>

const int buttons = 2; // Nr. of buttons in the array

const int PIN_LEFT_BUTTON = 4;//19; //D2
const int PIN_RIGHT_BUTTON = 5; //D1
const int PIN_MOTOR = 0; //D3

ClickButton button[2] = {
  ClickButton (PIN_LEFT_BUTTON, LOW, CLICKBTN_PULLUP),
  ClickButton (PIN_RIGHT_BUTTON, LOW, CLICKBTN_PULLUP)
};

const int MOTOR_OFF = 0;
const int MOTOR_ON = 1;
const int MOTOR_STANDBY = 2;

const int MOTOR_MAX_SPEED = 160;
const int MOTOR_MIN_SPEED = 30;
const int SPEED_UP_TIME_MS = 4000;

const int MOTOR_SPEED_CHANGE = MOTOR_MAX_SPEED*0.05;


int leftButtonState = 0;
int rightButtonState = 0;
int motorState = MOTOR_OFF;
int motorSpeed = 0;
int targetMotorSpeed = MOTOR_MIN_SPEED;
int motorStartTime = 0;

PinButton leftButton(PIN_LEFT_BUTTON);
PinButton rightButton(PIN_RIGHT_BUTTON);

Servo servo;



void setup() {
  
  for (int i=0; i<buttons; i++)
  {
    pinMode(ledPin[i],OUTPUT);  

    // Setup button timers (all in milliseconds / ms)
    // (These are default if not set, but changeable for convenience)
    button[i].debounceTime   = 20;   // Debounce timer in ms
    button[i].multiclickTime = 250;  // Time limit for multi clicks
    button[i].longClickTime  = 1000; // Time until long clicks register
  }

  Serial.begin(9600);

  servo.attach(PIN_MOTOR);
  servo.write(25); // needed for initializing the ESC
  delay(2000);

}

void updateMotorSpeed(){
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
  if(leftButtonState == 1 || rightButtonState == 1){
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
    motorSpeed = 0;
  }else if (motorState == MOTOR_ON){

    int diff = millis() - motorStartTime;
    int end_speed_up = SPEED_UP_TIME_MS * targetMotorSpeed / MOTOR_MAX_SPEED;
    if (diff >= end_speed_up){
      motorSpeed = targetMotorSpeed; 
    }else{
      motorSpeed = targetMotorSpeed * diff / end_speed_up;
    }
    servo.write(motorSpeed);
  }
  Serial.print(" motorSpeed: ");
  Serial.print(motorSpeed);
}

void loop() {
   for (int i=0; i<buttons; i++)
  {
    // Update state of all buitton
    button[i].Update();
  
    // Save click codes in LEDfunction, as clicks counts are reset at next Update()
    if (button[i].clicks != 0) LEDfunction[i] = button[i].clicks;
  

    // Simply toggle LED on single clicks
    // (Cant use LEDfunction like the others here,
    //  as it would toggle on and off all the time)
    if(button[i].clicks == 1) ledState[i] = !ledState[i];

    // blink faster if double clicked
    if(LEDfunction[i] == 2) ledState[i] = (millis()/500)%2;

    // blink even faster if triple clicked
    if(LEDfunction[i] == 3) ledState[i] = (millis()/200)%2;

    // slow blink (must hold down button. 1 second long blinks)
    if(LEDfunction[i] == -1) ledState[i] = (millis()/1000)%2;

    // slower blink (must hold down button. 2 second loong blinks)
    if(LEDfunction[i] == -2) ledState[i] = (millis()/2000)%2;

    // even slower blink (must hold down button. 3 second looong blinks)
    if(LEDfunction[i] == -3) ledState[i] = (millis()/3000)%2;
  }
  


  leftButtonState = digitalRead(PIN_LEFT_BUTTON);
  Serial.print("left: ");
  Serial.print(leftButtonState);

  rightButtonState = digitalRead(PIN_RIGHT_BUTTON);
  Serial.print(" right: ");
  Serial.print(rightButtonState);

  Serial.print(" millis: ");
  Serial.print(millis());

  updateMotorSpeed();
  controlMotor();

  Serial.println();

}
