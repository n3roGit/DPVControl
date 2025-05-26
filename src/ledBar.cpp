#include "ledBar.h"
#include <Adafruit_NeoPixel.h>
#include "constants.h"
#include "motor.h"

/**
* Code that controls the two led strips
*/

/*
*  CONSTANTS
*/
const int LedBar_Num = 10;          // Number of LEDs in the strip
const int LEDBar_Brightness = 15;
const int LEDBar_BrightnessSecond = 3;

/*
* GLOBAL VARIABLES 
*/
Adafruit_NeoPixel strip = Adafruit_NeoPixel(LedBar_Num + LedBar2_Num, PIN_LEDBAR, NEO_GRB + NEO_KHZ800);

// Variables to track last displayed state to prevent unnecessary updates
static int lastDisplayedSpeed = -1;
static int lastDisplayedMotorState = -1;
static int lastDisplayedBattery = -1;

void ledBarSetup(){
  //Neopixel
  strip.begin();
  strip.show();  // Turn off all LEDs
  setBarStandby();
}


void setBar(int stripNumber, int numLEDsOn, String hexColorOn, int brightnessOn, String hexColorOff, int brightnessOff) {
  // Make sure that stripNumber is valid (1 for the first strip, 2 for the second strip)
  if (stripNumber != 1 && stripNumber != 2) {
    return; // Unauthorized value, do nothing
  }

  // Calculate start index based on stripNumber
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
    // Reset cache when entering special mode
    lastDisplayedSpeed = -1;
    lastDisplayedMotorState = -1;
    setBar(1,10,"#e38f09", LEDBar_BrightnessSecond, "#000000", 0);
}

void setBarSpeed(int num) {
    // Only update if speed or motor state has changed
    if (num != lastDisplayedSpeed || motorState != lastDisplayedMotorState) {
        lastDisplayedSpeed = num;
        lastDisplayedMotorState = motorState;
        
        if (motorState == cruise) {
            setBarSpeedCruise(num);
        } else {
            setBar(1,num,"#cb1bf2", LEDBar_Brightness, "#000000", 0);
        }
    }
}

void setBarSpeedCruise(int num) {
    if (num <= 0) {
        setBar(1,0,"#000000", 0, "#000000", 0);
        return;
    }
    
    // Set all LEDs except the last one to pink
    setBar(1,num-1,"#cb1bf2", LEDBar_Brightness, "#000000", 0);
    
    // Set the last LED to red
    int startIndex = 0;
    int lastLEDIndex = startIndex + num - 1;
    strip.setPixelColor(lastLEDIndex, strip.Color(LEDBar_Brightness, 0, 0));
    strip.show();
}

void setBarBattery(int num) {
  // Only update if battery level has changed
  if (num != lastDisplayedBattery) {
    lastDisplayedBattery = num;
    int calc = LedBar_Num-num;
    setBar(2,calc,"#e30b0b", LEDBar_BrightnessSecond, "#0a9e08", LEDBar_Brightness);
  }
}

void setBarLeak() {
    // Reset cache when entering special mode
    lastDisplayedSpeed = -1;
    lastDisplayedMotorState = -1;
    
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

void setBarFlasher(bool status) {
  if (status) {
    // Reset cache when entering special mode
    lastDisplayedSpeed = -1;
    lastDisplayedMotorState = -1;
    setBar(1, 10, "#FFFFFF", LEDBar_Brightness, "#000000", 0); // All 10 LEDs white
  } else {
    // Reset cache when leaving special mode to force refresh
    lastDisplayedSpeed = -1;
    lastDisplayedMotorState = -1;
    // Don't do anything here - the status restoration is handled by the caller
  }  
}

// Function to force refresh of LED bar (invalidate cache)
void forceRefreshLedBar() {
  lastDisplayedSpeed = -1;
  lastDisplayedMotorState = -1;
  lastDisplayedBattery = -1;
}