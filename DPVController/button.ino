/**
* Control of the two levers.
*
*/


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
