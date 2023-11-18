/**
* Code that controls the two led strips
*/


/*
*  CONSTANTS
*/
const int LedBar_Num = 10;          // Number of LEDs in the strip
const int LEDBar_Brightness = 30;
const int LEDBar_BrightnessSecond = 3;

/*
* GLOBAL VARIABLES 
*/
Adafruit_NeoPixel strip = Adafruit_NeoPixel(LedBar_Num + LedBar2_Num, PIN_LEDBAR, NEO_GRB + NEO_KHZ800);

void ledBarSetup(){
  //Neopixel
  strip.begin();
  strip.show();  // Alle LEDs ausschalten
  setBarStandby();
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

void setBarPowerBank(bool status) {
  if (status){
      setBar(1,9,"#000000", 0, "#036ffc", LEDBar_Brightness);
  }
  else {
      setBar(1,9,"#000000", 0, "#ff0000", LEDBar_Brightness);
  }  
}

void setBarLED(int num) {
    int calc = LedBar_Num-num;
    setBar(1,calc,"#000000", 0, "#FFFFFF", LEDBar_Brightness);
}