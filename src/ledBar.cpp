#include "ledBar.h"
#include <Adafruit_NeoPixel.h>
#include "constants.h"
#include "motor.h"
#include "settings.h"

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
// Initialize with maximum possible LEDs - will be configured properly in setup
Adafruit_NeoPixel strip = Adafruit_NeoPixel(50 + LedBar2_Num, PIN_LEDBAR, NEO_GRB + NEO_KHZ800);

// Variables to track last displayed state to prevent unnecessary updates
static int lastDisplayedSpeed = -1;
static int lastDisplayedMotorState = -1;
static int lastDisplayedBattery = -1;

// Function to calculate brightness correction based on active RGB channels
// This ensures that all colors appear equally bright regardless of how many LEDs are active
int calculateBrightnessCorrectedValue(int red, int green, int blue, int targetBrightness) {
    // Count how many channels are significantly active (above 10% of max)
    int activeChannels = 0;
    if (red > 25) activeChannels++;      // > 10% of 255
    if (green > 25) activeChannels++;    // > 10% of 255  
    if (blue > 25) activeChannels++;     // > 10% of 255
    
    // Avoid division by zero
    if (activeChannels == 0) return targetBrightness;
    
    // Reduce brightness proportionally to number of active channels
    // Single channel (R, G, or B): 100% brightness
    // Two channels (RG, RB, GB): ~71% brightness  
    // Three channels (RGB): ~58% brightness
    float correctionFactor = 1.0 / sqrt(activeChannels);
    
    int correctedBrightness = (int)(targetBrightness * correctionFactor);
    
    // Ensure we don't go below minimum threshold or above maximum
    if (correctedBrightness < 1) correctedBrightness = 1;
    if (correctedBrightness > 100) correctedBrightness = 100;
    
    return correctedBrightness;
}

void ledBarSetup(){
  //Neopixel
  strip.begin();
  strip.show();  // Turn off all LEDs
  
  // Run Knight Rider startup animation
  knightRiderStartup();
  
  // Start normal operation
  setBarStandby();
}


void setBar(int stripNumber, int numLEDsOn, String hexColorOn, int brightnessOn, String hexColorOff, int brightnessOff) {
  // Make sure that stripNumber is valid (1 for the first strip, 2 for the second strip)
  if (stripNumber != 1 && stripNumber != 2) {
    return; // Unauthorized value, do nothing
  }

  // Calculate start index based on stripNumber
  int ledBarNum = getLedBarNum();
  int startIndex = (stripNumber == 1) ? 0 : ledBarNum;

  // Calculate the end index based on stripNumber
  int endIndex = (stripNumber == 1) ? ledBarNum : ledBarNum + LedBar2_Num;

  // Convert the hex color value to RGB color values for the switched-on color
  long numberOn = (long)strtol(&hexColorOn[1], NULL, 16);
  int redOn = numberOn >> 16;
  int greenOn = (numberOn >> 8) & 0xFF;
  int blueOn = numberOn & 0xFF;

  // Apply brightness correction for ON color
  int correctedBrightnessOn = calculateBrightnessCorrectedValue(redOn, greenOn, blueOn, brightnessOn);

  // Set the LEDs according to the specified brightness and colors
  for (int i = startIndex; i < startIndex + numLEDsOn; i++) {
    int dimmed_color_r = redOn * correctedBrightnessOn / 100;
    int dimmed_color_g = greenOn * correctedBrightnessOn / 100;
    int dimmed_color_b = blueOn * correctedBrightnessOn / 100;
    strip.setPixelColor(i, strip.Color(dimmed_color_r, dimmed_color_g, dimmed_color_b));
  }

    // Convert the hex color value to RGB color values for the switched off color
    long numberOff = (long)strtol(&hexColorOff[1], NULL, 16);
    int redOff = numberOff >> 16;
    int greenOff = (numberOff >> 8) & 0xFF;
    int blueOff = numberOff & 0xFF;

  // Apply brightness correction for OFF color
  int correctedBrightnessOff = calculateBrightnessCorrectedValue(redOff, greenOff, blueOff, brightnessOff);

  // Set the LEDs for the side that is switched off
  for (int i = startIndex + numLEDsOn; i < endIndex; i++) {
    strip.setPixelColor(i, strip.Color(redOff * correctedBrightnessOff / 100, greenOff * correctedBrightnessOff / 100, blueOff * correctedBrightnessOff / 100));
  }

  strip.show();  // Update LED strips
}

void setBarStandby() {
    // Reset cache when entering special mode
    lastDisplayedSpeed = -1;
    lastDisplayedMotorState = -1;
    lastDisplayedBattery = -1; // Reset battery cache too
    
    // Force immediate update with current settings
    int ledBarNum = getLedBarNum();
    int brightness = getLedBarBrightnessSecond();
    setBar(1, ledBarNum, "#e38f09", brightness, "#000000", 0);
}

void setBarSpeed(int num) {
    // Only update if speed or motor state has changed
    if (num != lastDisplayedSpeed || motorState != lastDisplayedMotorState) {
        lastDisplayedSpeed = num;
        lastDisplayedMotorState = motorState;
        
        if (motorState == cruise) {
            setBarSpeedCruise(num);
        } else {
            setBar(1, num, "#cb1bf2", getLedBarBrightness(), "#000000", 0);
        }
    }
}

void setBarSpeedCruise(int num) {
    if (num <= 0) {
        setBar(1,0,"#000000", 0, "#000000", 0);
        return;
    }
    
    // Set all LEDs except the last one to pink
    setBar(1, num-1, "#cb1bf2", getLedBarBrightness(), "#000000", 0);
    
    // Set the last LED to red with brightness correction
    int startIndex = 0;
    int lastLEDIndex = startIndex + num - 1;
    
    // Apply brightness correction for red color (255, 0, 0)
    int correctedRedBrightness = calculateBrightnessCorrectedValue(255, 0, 0, getLedBarBrightness());
    int redValue = 255 * correctedRedBrightness / 100;
    
    strip.setPixelColor(lastLEDIndex, strip.Color(redValue, 0, 0));
    strip.show();
}

void setBarBattery(int num) {
  // Only update if battery level has changed
  if (num != lastDisplayedBattery) {
    lastDisplayedBattery = num;
    int calc = getLedBarNum() - num;
    setBar(2, calc, "#e30b0b", getLedBarBrightnessSecond(), "#0a9e08", getLedBarBrightness());
  }
}

void setBarLeak() {
    // Reset cache when entering special mode
    lastDisplayedSpeed = -1;
    lastDisplayedMotorState = -1;
    
    int frontLeakState = digitalRead(PIN_LEAK_FRONT);
    int backLeakState = digitalRead(PIN_LEAK_BACK);

    if (backLeakState == LOW && frontLeakState == LOW) {
      setBar(1, getLedBarNum(), "#0000FF", getLedBarBrightness(), "#0000FF", 0);
    } else if (backLeakState == LOW) {
      setBar(1, getLedBarNum()/2, "#0000FF", getLedBarBrightness(), "#0000FF", 0);
    } else if(frontLeakState == LOW) {
      setBar(1, getLedBarNum()/2, "#0000FF", 0, "#0000FF", getLedBarBrightness());
    }
}

void setBarPowerBank(bool status) {
  int numLeds = getLedBarNum() - 1;
  if (status){
      setBar(1, numLeds, "#000000", 0, "#036ffc", getLedBarBrightness());
  }
  else {
      setBar(1, numLeds, "#000000", 0, "#ff0000", getLedBarBrightness());
  }  
}

void setBarLED(int num) {
    int calc = getLedBarNum() - num;
    setBar(1, calc, "#000000", 0, "#FFFFFF", getLedBarBrightness());
}

void setBarFlasher(bool status) {
  if (status) {
    // Reset cache when entering special mode
    lastDisplayedSpeed = -1;
    lastDisplayedMotorState = -1;
    setBar(1, getLedBarNum(), "#FFFFFF", getLedBarBrightness(), "#000000", 0); // All LEDs white
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

// Knight Rider startup animation
void knightRiderStartup() {
    const int delayTime = 60; // ms between steps
    const int maxBrightness = 80; // Maximum brightness for the effect
    const int stripLength = getLedBarNum(); // Length of each strip (usually 10)
    
    // Run the effect 2 times
    for (int cycle = 0; cycle < 2; cycle++) {
        
        // Forward sweep (left to right) on both strips simultaneously
        for (int pos = 0; pos < stripLength; pos++) {
            // Clear all LEDs
            for (int i = 0; i < stripLength + LedBar2_Num; i++) {
                strip.setPixelColor(i, strip.Color(0, 0, 0));
            }
            
            // LOWER STRIP (LEDs 0-9): Create trailing effect with 3 LEDs
            // Main LED (brightest)
            strip.setPixelColor(pos, strip.Color(maxBrightness, 0, 0));
            
            // Trailing LED 1 (medium brightness)
            if (pos > 0) {
                strip.setPixelColor(pos - 1, strip.Color(maxBrightness / 3, 0, 0));
            }
            
            // Trailing LED 2 (dim)
            if (pos > 1) {
                strip.setPixelColor(pos - 2, strip.Color(maxBrightness / 8, 0, 0));
            }
            
            // UPPER STRIP (LEDs 10-19): Same effect, offset by stripLength
            int upperPos = stripLength + pos;
            
            // Main LED (brightest)
            strip.setPixelColor(upperPos, strip.Color(maxBrightness, 0, 0));
            
            // Trailing LED 1 (medium brightness)
            if (pos > 0) {
                strip.setPixelColor(upperPos - 1, strip.Color(maxBrightness / 3, 0, 0));
            }
            
            // Trailing LED 2 (dim)
            if (pos > 1) {
                strip.setPixelColor(upperPos - 2, strip.Color(maxBrightness / 8, 0, 0));
            }
            
            strip.show();
            delay(delayTime);
        }
        
        // Backward sweep (right to left) on both strips simultaneously
        for (int pos = stripLength - 1; pos >= 0; pos--) {
            // Clear all LEDs
            for (int i = 0; i < stripLength + LedBar2_Num; i++) {
                strip.setPixelColor(i, strip.Color(0, 0, 0));
            }
            
            // LOWER STRIP (LEDs 0-9): Create trailing effect with 3 LEDs
            // Main LED (brightest)
            strip.setPixelColor(pos, strip.Color(maxBrightness, 0, 0));
            
            // Trailing LED 1 (medium brightness)
            if (pos < stripLength - 1) {
                strip.setPixelColor(pos + 1, strip.Color(maxBrightness / 3, 0, 0));
            }
            
            // Trailing LED 2 (dim)
            if (pos < stripLength - 2) {
                strip.setPixelColor(pos + 2, strip.Color(maxBrightness / 8, 0, 0));
            }
            
            // UPPER STRIP (LEDs 10-19): Same effect, offset by stripLength
            int upperPos = stripLength + pos;
            
            // Main LED (brightest)
            strip.setPixelColor(upperPos, strip.Color(maxBrightness, 0, 0));
            
            // Trailing LED 1 (medium brightness)
            if (pos < stripLength - 1) {
                strip.setPixelColor(upperPos + 1, strip.Color(maxBrightness / 3, 0, 0));
            }
            
            // Trailing LED 2 (dim)
            if (pos < stripLength - 2) {
                strip.setPixelColor(upperPos + 2, strip.Color(maxBrightness / 8, 0, 0));
            }
            
            strip.show();
            delay(delayTime);
        }
    }
    
    // Clear all LEDs after animation
    for (int i = 0; i < stripLength + LedBar2_Num; i++) {
        strip.setPixelColor(i, strip.Color(0, 0, 0));
    }
    strip.show();
    
    // Small pause before starting normal operation
    delay(200);
}