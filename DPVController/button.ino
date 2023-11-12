/**
* Control of the two levers.
*
*/

/**
* CONSTANTS
*/
const int DEBOUNCE_TIME = 20;
const int MULTICLICK_TIME = 600;
const int LONGCLICK_TIME = 10000;

/*
* GLOBAL VARIABLES
*/
int leftButtonState = DEPRESSED;
int rightButtonState = DEPRESSED;
ClickButton leftButton(PIN_LEFT_BUTTON, HIGH, CLICKBTN_PULLUP);
ClickButton rightButton(PIN_RIGHT_BUTTON, HIGH, CLICKBTN_PULLUP);
ClickButton getLeftButton(){return leftButton;};//Hack to access this from battery.ino


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
  if (leftButton.changed){
    log("leftButton.clicks", leftButton.clicks, EnableDebugLog);
  }
}