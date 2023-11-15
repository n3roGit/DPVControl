/**
* Control of the two levers.
*
*/

/**
* CONSTANTS
*/
const int DEBOUNCE_TIME = 20;
const int MULTICLICK_TIME = 300;
const int LONGCLICK_TIME = 1000;
const long NOT_HELD = -1;
const long MOTOR_START_DELAY = 300; // Time in ms that a lever needs
//to be hold down until the motor goes from off to on. 

/*
* GLOBAL VARIABLES
*/
int leftButtonState = DEPRESSED;
int rightButtonState = DEPRESSED;
int leftButtonHeldSince = NOT_HELD; //Time in MS at which 
//the button was held down and has not been released since. 
int rightButtonHeldSince = NOT_HELD; //Time in MS at which 
//the button was held down and has not been released since. 
ClickButton leftButton(PIN_LEFT_BUTTON, LOW);
ClickButton rightButton(PIN_RIGHT_BUTTON, LOW);

void buttonSetup(){
  // Set debounce and click times for buttons
  leftButton.debounceTime = DEBOUNCE_TIME;
  leftButton.multiclickTime = MULTICLICK_TIME;
  leftButton.longClickTime = LONGCLICK_TIME;
  rightButton.debounceTime = DEBOUNCE_TIME;     
  rightButton.multiclickTime = MULTICLICK_TIME; 
  rightButton.longClickTime = LONGCLICK_TIME;
}


void buttonLoop(){
  leftButton.Update();
  rightButton.Update();
  leftButtonState = digitalRead(PIN_LEFT_BUTTON);
  if(leftButtonState == PRESSED){
    if(leftButtonHeldSince == NOT_HELD){
      leftButtonHeldSince = millis();
    }
  }else{
    leftButtonHeldSince = NOT_HELD;
  }
  rightButtonState = digitalRead(PIN_RIGHT_BUTTON);
  if(rightButtonState == PRESSED){
    if(rightButtonHeldSince == NOT_HELD){
      rightButtonHeldSince = millis();
    }
  }else{
    rightButtonHeldSince = NOT_HELD;
  }

  if (motorState == MOTOR_STANDBY) {
    //Wake up from Standup
    if (leftButton.clicks == 2 || rightButton.clicks == 2) {
        wakeUp();
    }
  }else{
    if (rightButton.clicks == 2) {
      speedUp();
    }
    if (leftButton.clicks == 2) {
      speedDown();
    }  

    if (heldForLong(leftButtonHeldSince) || heldForLong(rightButtonHeldSince)) {
      motorState = MOTOR_ON;
    } else if(motorState == MOTOR_ON //When motor already on, holding one down to keep it on is
    //enough. 
      && (leftButtonState == PRESSED || rightButtonState == PRESSED)) {
      motorState = MOTOR_ON;
    }else{
      motorState = MOTOR_OFF;
    }
  
    if (leftButtonState == PRESSED || rightButtonState == PRESSED) {
      //While not in standby, any button click updates the standby counter.
      lastActionTime = micros();
    }
  }

  if (rightButton.clicks == 3) {
    toggleLED();
  }
  if (leftButton.clicks == 3) {
    outputBatteryInfo();
  }

}

bool heldForLong(long heldDownSince){
  return heldDownSince != NOT_HELD && millis()-heldDownSince >= MOTOR_START_DELAY;
}
