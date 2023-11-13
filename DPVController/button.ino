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

/*
* GLOBAL VARIABLES
*/
int leftButtonState = DEPRESSED;
int rightButtonState = DEPRESSED;
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
  rightButtonState = digitalRead(PIN_RIGHT_BUTTON);
  //log("rightButtonState", rightButtonState, EnableDebugLog);
  //log("leftButtonState", leftButtonState, EnableDebugLog);


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

    if (leftButtonState == PRESSED || rightButtonState == PRESSED) {
      motorState = MOTOR_ON;
    } else {
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
