/**
*
* Methods that control motor and speed.
*/

// Function to update the motor speed setting
void updateSpeedSetting() {
  if (motorState != MOTOR_STANDBY) {
    if (rightButton.clicks == -2) {
      log("rightButton.clicks", rightButton.clicks, true);
      rightButton.clicks = 0;
      speedSetting += MOTOR_SPEED_CHANGE;
      if (speedSetting > MOTOR_MAX_SPEED) {
        speedSetting = MOTOR_MAX_SPEED;
      }
      log("speedSetting", speedSetting, true);
      currentMotorStep = (currentMotorStep < 10) ? currentMotorStep + 1 : 10;
      setBarSpeed(currentMotorStep);
    }

    if (leftButton.clicks == -2) {
      log("leftButton.clicks", leftButton.clicks, true);
      speedSetting -= MOTOR_SPEED_CHANGE;
      if (speedSetting < MOTOR_MIN_SPEED) {
        speedSetting = MOTOR_MIN_SPEED;
      }
      log("speedSetting", speedSetting, true);
      currentMotorStep = (currentMotorStep > 1) ? currentMotorStep - 1 : 1;
      setBarSpeed(currentMotorStep);
    }
  }
}

// Function to control standby mode
void controlStandby() {
  if (motorState == MOTOR_STANDBY) {
    //Wake up from Standup
    if (leftButton.clicks == -2 || rightButton.clicks == -2) {
      motorState = MOTOR_OFF;
      log("leaving standby", 1, true);
      lastActionTime = micros();
      beep("2");
      setBarSpeed(currentMotorStep);
    }
  } else {
    if (lastActionTime + STANDBY_DELAY_MS < micros()) {
      //Go into standby
      log("going to standby", micros(), true);
      motorState = MOTOR_STANDBY;
      beep("2");
      setBarStandby();
    }
    if (leftButtonState == PRESSED || rightButtonState == PRESSED) {
      //While not in standby, any button click updates the standby counter.
      lastActionTime = micros();
      log("update lastActionTime", lastActionTime, EnableDebugLog);
    }
  }
}

void controlMotor() {
  if (motorState != MOTOR_STANDBY) {
    if (leftButtonState == PRESSED || rightButtonState == PRESSED) {
      motorState = MOTOR_ON;
    } else {
      motorState = MOTOR_OFF;
    }
  }
  //log("motorstate", motorState, EnableDebugLog);

  if (motorState == MOTOR_STANDBY || motorState == MOTOR_OFF) {
    // Motor is off
    targetMotorSpeed = 0;
  } else if (motorState == MOTOR_ON) {
    targetMotorSpeed = speedSetting;
  }
  // Serial.print(" targetMotorSpeed: ");
  // Serial.print(targetMotorSpeed);

  setSoftMotorSpeed();
}


/*
is this a clever solution to prevent overload?
*/
void PreventOverload() {

  if (LED_State == 3) {
    OverloadLimit = OverloadLimitMax - 3;
  } else if (LED_State == 4) {
    OverloadLimit = OverloadLimitMax - 4;
  } else {
    OverloadLimit = OverloadLimitMax;
  }

/*
// something like this to prevent overload. so i can limit the motor speed if i have other devices consuming current
  if (UART.data.avgInputCurrent >= OverloadLimit) {
    speedSetting -= MOTOR_SPEED_CHANGE;
  }  else if (UART.data.avgInputCurrent < OverloadLimit) {
    speedSetting += MOTOR_SPEED_CHANGE;
  }
  */
}



/**
* Slowly changes the motor speed to targetMotorSpeed.
*
**/
void setSoftMotorSpeed() {

  float timePassedSinceLastChange = micros() - currentMotorTime;
  bool speedUp = currentMotorSpeed < targetMotorSpeed;

  if (speedUp) {
    float maxChange = timePassedSinceLastChange * MOTOR_MAX_SPEED / SPEED_UP_TIME_MS;
    currentMotorSpeed += maxChange;
    currentMotorSpeed = min(currentMotorSpeed, targetMotorSpeed);
  } else {
    float maxChange = timePassedSinceLastChange * MOTOR_MAX_SPEED / SPEED_DOWN_TIME_MS;
    currentMotorSpeed -= maxChange;
    currentMotorSpeed = max(currentMotorSpeed, targetMotorSpeed);
  }
  //log("currentMotorSpeed", currentMotorSpeed, EnableDebugLog);
  if(currentMotorSpeed != 0){
    UART.setRPM(currentMotorSpeed);
  }
  
  currentMotorTime = micros();
}
