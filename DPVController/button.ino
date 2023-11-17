/**
* Control of the two levers.
*
*/

struct LastClick {
  unsigned long time;
  int clicks;
};

/**
* CONSTANTS
*/
const int DEBOUNCE_TIME = 20;
const int MULTICLICK_TIME = 300;
const int LONGCLICK_TIME = 1000;
const long NOT_HELD = -1;
const long MOTOR_START_DELAY = 300; // Time in ms that a lever needs
//to be hold down until the motor goes from off to on. 
const long BOTH_HAND_CLICK_TOLERANCE = 150; //maximum time in ms
//between the end of clicks on two separate buttons for them to 
//be considered "at the same time".

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
LastClick lastLeftClick;
LastClick lastRightClick;

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
  updateButtonState();
  performActions();
}

void updateLastClick(LastClick &click, ClickButton &button); // Forward declaration
bool checkCruise(ClickButton &button, LastClick &lastClick);

void updateButtonState(){
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
  updateLastClick(lastLeftClick, leftButton);
  rightButtonState = digitalRead(PIN_RIGHT_BUTTON);
  if(rightButtonState == PRESSED){
    if(rightButtonHeldSince == NOT_HELD){
      rightButtonHeldSince = millis();
    }
  }else{
    rightButtonHeldSince = NOT_HELD;
  }
  updateLastClick(lastRightClick, rightButton);
}


void performActions(){

  if (motorState == standby) {
    //Wake up from Standup
    if (leftButton.clicks == 2 || rightButton.clicks == 2) {
        wakeUp();
    }
  }else if(motorState == cruise){
    if (leftButton.changed || rightButton.changed){
      leaveCruiseMode();
    }
  }else{
    if (rightButton.clicks == 2 && leftButtonState == PRESSED) {
      speedUp();
    }
    if (leftButton.clicks == 2  && rightButtonState == PRESSED) {
      speedDown();
    }  

    if (heldForLong(leftButtonHeldSince) || heldForLong(rightButtonHeldSince)) {
      motorState = on;
    }else if(motorState == on //When motor already on, holding one down to keep it on is
    //enough. 
      && (leftButtonState == PRESSED || rightButtonState == PRESSED)) {
      motorState = on;
    }else if (checkCruise(rightButton, lastLeftClick)
            ||checkCruise(leftButton,  lastRightClick)){
      enterCruiseMode(); 
    }else{
      motorState = off;
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

void updateLastClick(LastClick &click, ClickButton &button){
  if (button.clicks != 0){
    click.time = millis();
    click.clicks = button.clicks;
  }
}

/**
*  Checks our condition for entering cruise mode.
*  But considering only one button and the other ones lastClick
*/
bool checkCruise(ClickButton &button, LastClick &lastClick){
  return button.clicks == 1 && lastClick.clicks == 1 
    && millis() - lastClick.time <= BOTH_HAND_CLICK_TOLERANCE;
}
