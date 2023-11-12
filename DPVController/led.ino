/***
*  Code that controls the two LED-Strips
*
*
***/



void controlLED() {
  if (rightButton.clicks == -3) {
    log("rightButton.clicks", rightButton.clicks, EnableDebugLog);

    switch (LED_State) {
      case 0:
        LED_State = 1;
        break;
      case 1:
        LED_State = 2;
        break;
      case 2:
        LED_State = 3;
        break;
      case 3:
        LED_State = 4;
        break;
      case 4:
        LED_State = 0;
        break;
      default:
        // If an invalid state is somehow reached, turn the LED off
        LED_State = 0;
        break;
    }
    setLEDState(LED_State);
    setBarLED(LED_State);
    log("LED_State", LED_State, true);
  }
}



void setLEDState(int state) {

  int brightness;
  switch (state) {
    case 0:
      brightness = 0;
      break;
    case 1:
      brightness = 20;
      break;
    case 2:
      brightness = 76;
      break;
    case 3:
      brightness = 153;
      break;
    case 4:
      brightness = 255;
      break;
    default:
      // If an invalid state is provided, assume 0% brightness
      brightness = 0;
      break;
  }
  /*
  Is it possible to change pwm frequency to advoid led flickering while filming 
  */
  //analogWrite(PIN_LED, brightness);  // LED-PIN, Brightness 0-255
  ledcWrite(LEDchannel, brightness);  // Set LED brightness using PWM
}


/*
Its working but while blinking the esp doesnt response
*/
void blinkLED(const String& sequence) {
  for (char c : sequence) {
    setLEDState(0);
    int blinkDuration = (c == '1') ? 200 : 600;
    setLEDState(4);
    float startMicros = micros();
    while (micros() - startMicros < blinkDuration * 1000) {
      // Wait until the desired duration is reached
    }
    setLEDState(0);
    lastBlinkTime = micros();
    while (micros() - lastBlinkTime < 400000) {
      // Pause between blinking
    }
  }
  //todo: after blink set led to the last state of LED_State after 5 seconds
}




void setBar(int stripNumber, int numLEDsOn, String hexColorOn, int brightnessOn, String hexColorOff, int brightnessOff) {
  // Make sure that stripNumber is valid (1 for the first strip, 2 for the second strip)
  if (stripNumber != 1 && stripNumber != 2) {
    return; // Unauthorized value, do nothing
  }

  // Berechne den Startindex basierend auf stripNumber
  int startIndex = (stripNumber == 1) ? 0 : LedBar_Num;

  // Calculate the end index based on stripNumber
  int endIndex = (stripNumber == 1) ? LedBar_Num : LedBar_Num + LedBar2_Num;

  // Convert the hex color value to RGB color values for the switched-on color
  long numberOn = (long)strtol(&hexColorOn[1], NULL, 16);
  int redOn = numberOn >> 16;
  int greenOn = (numberOn >> 8) & 0xFF;
  int blueOn = numberOn & 0xFF;

  // Set the LEDs according to the specified brightness and colors
  for (int i = startIndex; i < startIndex + numLEDsOn; i++) {
    int dimmed_color_r = redOn * brightnessOn / 100;
    int dimmed_color_g = greenOn * brightnessOn / 100;
    int dimmed_color_b = blueOn * brightnessOn / 100;
    strip.setPixelColor(i, strip.Color(dimmed_color_r, dimmed_color_g, dimmed_color_b));
  }

  // Set the LEDs for the side that is switched off
  for (int i = startIndex + numLEDsOn; i < endIndex; i++) {
    // Convert the hex color value to RGB color values for the switched off color
    long numberOff = (long)strtol(&hexColorOff[1], NULL, 16);
    int redOff = numberOff >> 16;
    int greenOff = (numberOff >> 8) & 0xFF;
    int blueOff = numberOff & 0xFF;
    strip.setPixelColor(i, strip.Color(redOff * brightnessOff / 100, greenOff * brightnessOff / 100, blueOff * brightnessOff / 100));
  }

  strip.show();  // Update LED strips
}

void setBarStandby() {
    setBar(1,10,"#e38f09", LEDBar_BrightnessSecond, "#000000", 0);
}

void setBarSpeed(int num) {
    setBar(1,num,"#cb1bf2", LEDBar_Brightness, "#000000", 0);
}

void setBarBattery(int num) {
  int calc = LedBar_Num-num;
  setBar(2,calc,"#e30b0b", LEDBar_BrightnessSecond, "#0a9e08", LEDBar_Brightness);
}

void setBarLeak() {
    int frontLeakState = digitalRead(PIN_LEAK_FRONT);
    int backLeakState = digitalRead(PIN_LEAK_BACK);

    if (backLeakState == LOW && frontLeakState == LOW) {
      setBar(1,10,"#0000FF", LEDBar_Brightness, "#0000FF", 0);
    } else if (backLeakState == LOW) {
      setBar(1,5,"#0000FF", LEDBar_Brightness, "#0000FF", 0);
    } else if(frontLeakState == LOW) {
      setBar(1,5,"#0000FF", 0, "#0000FF", LEDBar_Brightness);
    }
}

void setBarLED(int num) {
    int calc = LedBar_Num-num;
    setBar(1,calc,"#000000", 0, "#FFFFFF", LEDBar_Brightness);
}

void BlinkForLongStandby() {
  if (motorState == MOTOR_STANDBY && micros() - lastStandbyBlinkTime >= StandbyBlinkWarningtime && LED_State == 0) {
    blinkLED("111222111");  // Hier die gewünschte Sequenz für den Ton
    log("sos iam alone", 111222111, true);
    StandbyBlinkWarningtime = (StandbyBlinkDuration * 1000000);  //every 30 sec
    lastStandbyBlinkTime = micros();                             // Update the time of the last call
  } else {
    StandbyBlinkWarningtime = (StandbyBlinkStart * 60 * 1000000);  //every 3 minutes
  }
}
