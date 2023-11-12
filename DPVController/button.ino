/**
* Control of the two levers.
*
*/


/*
* GLOBAL VARIABLES
*/
int leftButtonState = 0;
int rightButtonState = 0;
ClickButton leftButton(PIN_LEFT_BUTTON, HIGH, CLICKBTN_PULLUP);
ClickButton rightButton(PIN_RIGHT_BUTTON, HIGH, CLICKBTN_PULLUP);
ClickButton getLeftButton(){return leftButton;};//Hack to access this from battery.ino


void buttonSetup(){
  // Set debounce and click times for buttons
  leftButton.debounceTime = 40;
  leftButton.multiclickTime = 300;
  leftButton.longClickTime = 1000;
  rightButton.debounceTime = 40;     //20
  rightButton.multiclickTime = 300;  //500
  rightButton.longClickTime = 1000;
}


void buttonLoop(){
  leftButton.Update();
  rightButton.Update();

  leftButtonState = digitalRead(PIN_LEFT_BUTTON);
  rightButtonState = digitalRead(PIN_RIGHT_BUTTON);

  //log("rightButtonState", rightButtonState, EnableDebugLog);
  //log("leftButtonState", leftButtonState, EnableDebugLog);
  //checkButtonClicks();
}

/*
idea to check all the click codes in one funtion. not working at the moment
*/
void checkButtonClicks() {
  leftButton.Update();
  rightButton.Update();


  if (rightButtonState == 0) {
    if (leftButton.clicks == 1) {
      Serial.println("1 Click - Hold");
    } else if (leftButton.clicks == 2) {
      Serial.println("2 Clicks - Hold");
    } else if (leftButton.clicks == 3) {
      Serial.println("3 Clicks - Hold");
    }
    if (leftButtonState == 0) {
      if (rightButton.clicks == 1) {
        Serial.println("Hold - 1 Click");
      } else if (rightButton.clicks == 2) {
        Serial.println("Hold - 2 Clicks");
      } else if (rightButton.clicks == 3) {
        Serial.println("Hold - 3 Clicks");
      }
    }
  }



  if (leftButton.clicks == 1 && rightButton.clicks == 1) {
    Serial.println("1 Click - 1 Click");
  } else if (leftButton.clicks == 2 && rightButton.clicks == 2) {
    Serial.println("2 Clicks - 2 Clicks");
  } else if (leftButton.clicks == 3 && rightButton.clicks == 3) {
    Serial.println("3 Clicks - 3 Clicks");
  } else if (leftButton.clicks == 1 && rightButtonState == 1) {
    Serial.println("1 Click -");
  } else if (rightButton.clicks == 1 && leftButtonState == 1) {
    Serial.println("- 1 Click");
  }
}
