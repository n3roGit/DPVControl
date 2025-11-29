#include "ledBar.h"
#include <Adafruit_NeoPixel.h>
#include "constants.h"
#include "motor.h"
#include "settings.h"
#include "log.h"
#include "battery.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

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

// Real mutex to prevent concurrent LED updates across cores/tasks
static SemaphoreHandle_t ledBarMutex = nullptr;
static volatile unsigned long lastStripShowMicros = 0;
static const unsigned long MIN_UPDATE_INTERVAL_US = 1000; // Rate limit strip.show() calls

// Critical section mux for LED timing
static portMUX_TYPE ledMux = portMUX_INITIALIZER_UNLOCKED;

static bool tryLockLedBar() {
  if (ledBarMutex == nullptr) {
    return true; // If not initialized yet, proceed (setup will create it)
  }
  return xSemaphoreTake(ledBarMutex, 0) == pdTRUE;
}

static void unlockLedBar() {
  if (ledBarMutex != nullptr) {
    xSemaphoreGive(ledBarMutex);
  }
}

static void safeStripShow() {
  unsigned long now = micros();
  unsigned long elapsed = now - lastStripShowMicros;
  if (elapsed < MIN_UPDATE_INTERVAL_US) {
    delayMicroseconds(MIN_UPDATE_INTERVAL_US - elapsed);
  }
  
  // Critical section removed due to boot loop issues with PSRAM/GPIO12
  // portENTER_CRITICAL(&ledMux);
  strip.show();
  // portEXIT_CRITICAL(&ledMux);
  
  lastStripShowMicros = micros();
}

static volatile bool ledBarUpdateRequested = false;

void requestLedBarUpdate() {
    ledBarUpdateRequested = true;
}

void ledBarLoop() {
    if (ledBarUpdateRequested) {
        ledBarUpdateRequested = false;
        
        // Re-apply current state based on motor state
        // Note: motorState and currentMotorStep are externs available via motor.h
        if (motorState == standby) {
            setBarStandby();
        } else {
            setBarSpeed(currentMotorStep);
        }
    }
}

// Helper function to safely get LED strip boundaries
void getStripBoundaries(int stripNumber, int& startIndex, int& endIndex) {
    int ledBarNum = getLedBarNum();
    if (ledBarNum == 0) ledBarNum = 10; // Fallback
    
    if (stripNumber == 1) {
        startIndex = 0;
        endIndex = ledBarNum;
    } else if (stripNumber == 2) {
        startIndex = ledBarNum;
        endIndex = ledBarNum + LedBar2_Num;
    } else {
        // Invalid strip number
        startIndex = 0;
        endIndex = 0;
    }
}

// Helper function to safely set a single LED with boundary checking
bool safeSetPixelColor(int index, uint32_t color) {
    int totalLEDs = getLedBarNum() + LedBar2_Num;
    if (totalLEDs == 0) totalLEDs = 20; // Fallback
    
    if (index >= 0 && index < totalLEDs) {
        strip.setPixelColor(index, color);
        return true;
    } else {
        log("ERROR: LED index " + String(index) + " out of bounds (0-" + String(totalLEDs-1) + ")");
        return false;
    }
}

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
  // Neopixel
  strip.begin();
  // Initialize mutex
  if (ledBarMutex == nullptr) {
    ledBarMutex = xSemaphoreCreateMutex();
  }
  
  // Explicitly clear both strips first
  int ledBarNum = getLedBarNum();
  if (ledBarNum == 0) ledBarNum = 10; // Fallback
  int totalLEDs = ledBarNum + LedBar2_Num;
  
  if (tryLockLedBar()) {
    for (int i = 0; i < totalLEDs; i++) {
      safeSetPixelColor(i, strip.Color(0, 0, 0));
    }
    safeStripShow(); // Turn off all LEDs
    unlockLedBar();
  }
  
  // Proper delay to ensure LEDs are initialized
  delay(250);
  
  // Run Knight Rider startup animation
  knightRiderStartup();
  
  // Single thorough clearing after animation
  if (tryLockLedBar()) {
    for (int i = 0; i < totalLEDs; i++) {
      safeSetPixelColor(i, strip.Color(0, 0, 0));
    }
    safeStripShow();
    unlockLedBar();
  }
  
  // Force refresh to clear any cached states
  forceRefreshLedBar();
  
  // Adequate delay before setting states
  delay(100);
  
  // Set standby state on Strip 1 first
  setBarStandby();
  
  // Delay between strip operations
  delay(75);
  
  // Then initialize battery display on Strip 2 (this must come after setBarStandby)
  // Force battery display update by getting current level and setting it
  int steps = (batteryLevel + 5) / LedBar2_Num;
  steps = constrain(steps, 0, LedBar2_Num);
  
  // Reset battery cache to force update
  lastDisplayedBattery = -1;
  setBarBattery(steps);
  
  // Final delay to ensure display is stable
  delay(100);
}


void setBar(int stripNumber, int numLEDsOn, uint32_t colorOn, int brightnessOn, uint32_t colorOff, int brightnessOff, bool immediateShow) {
  // Prevent concurrent updates
  if (!tryLockLedBar()) {
    log("WARNING: LED update already in progress, skipping");
    return;
  }
  
  // Make sure that stripNumber is valid (1 for the first strip, 2 for the second strip)
  if (stripNumber != 1 && stripNumber != 2) {
    log("ERROR: Invalid stripNumber: " + String(stripNumber));
    unlockLedBar();
    return; // Unauthorized value, do nothing
  }

  // Get safe strip boundaries
  int startIndex, endIndex;
  getStripBoundaries(stripNumber, startIndex, endIndex);
  
  // Validate numLEDsOn against strip boundaries
  int maxLEDs = endIndex - startIndex;
  numLEDsOn = constrain(numLEDsOn, 0, maxLEDs);

  // Extract RGB from uint32_t
  int redOn = (uint8_t)(colorOn >> 16);
  int greenOn = (uint8_t)(colorOn >> 8);
  int blueOn = (uint8_t)colorOn;

  // Apply brightness correction for ON color
  int correctedBrightnessOn = calculateBrightnessCorrectedValue(redOn, greenOn, blueOn, brightnessOn);

  // Set the LEDs according to the specified brightness and colors
  for (int i = startIndex; i < startIndex + numLEDsOn; i++) {
    int dimmed_color_r = redOn * correctedBrightnessOn / 100;
    int dimmed_color_g = greenOn * correctedBrightnessOn / 100;
    int dimmed_color_b = blueOn * correctedBrightnessOn / 100;
    safeSetPixelColor(i, strip.Color(dimmed_color_r, dimmed_color_g, dimmed_color_b));
  }

  // Extract RGB from uint32_t
  int redOff = (uint8_t)(colorOff >> 16);
  int greenOff = (uint8_t)(colorOff >> 8);
  int blueOff = (uint8_t)colorOff;

  // Apply brightness correction for OFF color
  int correctedBrightnessOff = calculateBrightnessCorrectedValue(redOff, greenOff, blueOff, brightnessOff);

  // Set the LEDs for the side that is switched off
  for (int i = startIndex + numLEDsOn; i < endIndex; i++) {
    safeSetPixelColor(i, strip.Color(redOff * correctedBrightnessOff / 100, greenOff * correctedBrightnessOff / 100, blueOff * correctedBrightnessOff / 100));
  }

  if (immediateShow) {
    safeStripShow();  // Update LED strips
  }
  unlockLedBar();
}

void setBarStandby(bool immediateShow) {
    // Reset cache when entering special mode (only for Strip 1)
    lastDisplayedSpeed = -1;
    lastDisplayedMotorState = -1;
    // Don't reset lastDisplayedBattery as it's for Strip 2 and should not be affected
    
    // Force immediate update with current settings
    int ledBarNum = getLedBarNum();
    int brightness = getLedBarBrightnessSecond();
    
    // Fallback values if settings are not loaded yet
    if (ledBarNum == 0) ledBarNum = 10;  // Default LED bar length
    if (brightness == 0) brightness = 3; // Default brightness
    
    setBar(1, ledBarNum, 0xe38f09, brightness, 0x000000, 0, immediateShow);
}

void setBarSpeed(int num, bool immediateShow) {
    // Only update if speed or motor state has changed
    if (num != lastDisplayedSpeed || motorState != lastDisplayedMotorState) {
        lastDisplayedSpeed = num;
        lastDisplayedMotorState = motorState;
        
        if (motorState == cruise) {
            setBarSpeedCruise(num, immediateShow);
        } else {
            setBar(1, num, 0xcb1bf2, getLedBarBrightness(), 0x000000, 0, immediateShow);
        }
    }
}

void setBarSpeedCruise(int num, bool immediateShow) {
    if (num <= 0) {
        setBar(1, 0, 0x000000, 0, 0x000000, 0, immediateShow);
        return;
    }
    
    // Get strip boundaries for validation
    int startIndex, endIndex;
    getStripBoundaries(1, startIndex, endIndex);
    int maxLEDs = endIndex - startIndex;
    
    // Validate num against strip boundaries
    num = constrain(num, 1, maxLEDs);
    
    // Prevent concurrent updates
    if (!tryLockLedBar()) {
        log("WARNING: LED update already in progress in setBarSpeedCruise");
        return;
    }
    
    // Clear all LEDs in strip 1 first
    for (int i = startIndex; i < endIndex; i++) {
        safeSetPixelColor(i, strip.Color(0, 0, 0));
    }
    
    // Set all LEDs except the last one to pink
    for (int i = startIndex; i < startIndex + num - 1; i++) {
        int correctedBrightness = calculateBrightnessCorrectedValue(203, 27, 242, getLedBarBrightness());
        int dimmed_r = 203 * correctedBrightness / 100;
        int dimmed_g = 27 * correctedBrightness / 100;
        int dimmed_b = 242 * correctedBrightness / 100;
        safeSetPixelColor(i, strip.Color(dimmed_r, dimmed_g, dimmed_b));
    }
    
    // Set the last LED to red with boundary check
    int lastLEDIndex = startIndex + num - 1;
    if (lastLEDIndex >= startIndex && lastLEDIndex < endIndex) {
        int correctedRedBrightness = calculateBrightnessCorrectedValue(255, 0, 0, getLedBarBrightness());
        int redValue = 255 * correctedRedBrightness / 100;
        safeSetPixelColor(lastLEDIndex, strip.Color(redValue, 0, 0));
    }
    
    if (immediateShow) {
        safeStripShow();
    }
    unlockLedBar();
}

void setBarBattery(int num, bool immediateShow) {
  // Only update if battery level has changed
  if (num != lastDisplayedBattery) {
    lastDisplayedBattery = num;
    int calc = getLedBarNum() - num;
    setBar(2, calc, 0xe30b0b, getLedBarBrightnessSecond(), 0x0a9e08, getLedBarBrightness(), immediateShow);
  }
}

void setBarLeak(bool immediateShow) {
    // Reset cache when entering special mode
    lastDisplayedSpeed = -1;
    lastDisplayedMotorState = -1;
    
    int frontLeakState = digitalRead(PIN_LEAK_FRONT);
    int backLeakState = digitalRead(PIN_LEAK_BACK);

    if (backLeakState == LOW && frontLeakState == LOW) {
      setBar(1, getLedBarNum(), 0x0000FF, getLedBarBrightness(), 0x0000FF, 0, immediateShow);
    } else if (backLeakState == LOW) {
      setBar(1, getLedBarNum()/2, 0x0000FF, getLedBarBrightness(), 0x0000FF, 0, immediateShow);
    } else if(frontLeakState == LOW) {
      setBar(1, getLedBarNum()/2, 0x0000FF, 0, 0x0000FF, getLedBarBrightness(), immediateShow);
    }
}

void setBarPowerBank(bool status, bool immediateShow) {
  int numLeds = getLedBarNum() - 1;
  if (status){
      setBar(1, numLeds, 0x000000, 0, 0x036ffc, getLedBarBrightness(), immediateShow);
  }
  else {
      setBar(1, numLeds, 0x000000, 0, 0xff0000, getLedBarBrightness(), immediateShow);
  }  
}

void setBarLED(int num, bool immediateShow) {
    int ledBarNum = getLedBarNum();
    if (ledBarNum == 0) ledBarNum = 10; // Fallback
    
    // For lamp levels: 0=OFF (0 LEDs), 1-4 = brightness levels (1-4 LEDs)
    // We want the LEDs to build up from right to left
    int numOn = num; // num is already the correct number of LEDs to show
    int numOff = ledBarNum - numOn;
    
    // Display: OFF LEDs on left (black), ON LEDs on right (white)
    // This makes the brightness build up from right to left
    setBar(1, numOff, 0x000000, 0, 0xFFFFFF, getLedBarBrightness(), immediateShow);
}

void setBarFlasher(bool status, bool immediateShow) {
  if (status) {
    // Reset cache when entering special mode
    lastDisplayedSpeed = -1;
    lastDisplayedMotorState = -1;
    setBar(1, getLedBarNum(), 0xFFFFFF, getLedBarBrightness(), 0x000000, 0, immediateShow); // All LEDs white
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

// Function to apply LED bar settings changes at runtime
void applyLedBarSettings() {
  log("Applying LED bar settings changes...");
  
  // Force refresh of all LED displays to use new settings
  forceRefreshLedBar();
  
  // Re-apply current state with new settings
  extern int currentMotorStep;
  extern MotorState motorState;
  extern int batteryLevel;
  
  // Update LED bar display based on current motor state
  if (motorState == standby) {
    setBarStandby();
  } else {
    setBarSpeed(currentMotorStep);
  }
  
  // Update battery display
  int steps = (batteryLevel + 5) / LedBar2_Num;
  steps = constrain(steps, 0, LedBar2_Num);
  setBarBattery(steps);
  
  String settingsMsg = "LED bar settings applied - LEDs: " + String(getLedBarNum()) + 
                      ", Brightness: " + String(getLedBarBrightness()) + 
                      ", Brightness2: " + String(getLedBarBrightnessSecond());
  log(settingsMsg.c_str());
}

// Knight Rider startup animation
void knightRiderStartup() {
  const int delayTime = 30; // ms between steps 
  const int maxBrightness = 80; // Maximum brightness for the effect
  int ledBarNum = getLedBarNum();
  if (ledBarNum == 0) ledBarNum = 10; // Fallback if settings not loaded
  
  // Prevent concurrent updates
  if (!tryLockLedBar()) {
    log("WARNING: LED update already in progress during Knight Rider");
    return;
  }
  
  // Calculate total LED count for boundary checking
  int totalLEDs = ledBarNum + LedBar2_Num;
  
  // Run the effect 2 times
  for (int cycle = 0; cycle < 2; cycle++) {
    // Forward sweep: Strip 1 (left to right), Strip 2 (right to left)
    for (int pos = 0; pos < ledBarNum; pos++) {
      // Clear all LEDs with boundary check
      for (int i = 0; i < totalLEDs; i++) {
        safeSetPixelColor(i, strip.Color(0, 0, 0));
      }

      // Strip 1 (lower strip): left to right (positions 0 to ledBarNum-1)
      safeSetPixelColor(pos, strip.Color(maxBrightness, 0, 0)); // Main LED
      if (pos > 0) {
        safeSetPixelColor(pos - 1, strip.Color(maxBrightness / 3, 0, 0)); // Trailing LED 1
      }
      if (pos > 1) {
        safeSetPixelColor(pos - 2, strip.Color(maxBrightness / 8, 0, 0)); // Trailing LED 2
      }
      
      // Strip 2 (upper strip): right to left (positions ledBarNum to ledBarNum+LedBar2_Num-1)
      int strip2Pos = ledBarNum + (LedBar2_Num - 1 - pos); // Fixed mirror position calculation
      if (strip2Pos >= ledBarNum && strip2Pos < totalLEDs) { // Boundary check
        safeSetPixelColor(strip2Pos, strip.Color(maxBrightness, 0, 0)); // Main LED
        if (pos > 0 && strip2Pos + 1 < totalLEDs) {
          safeSetPixelColor(strip2Pos + 1, strip.Color(maxBrightness / 3, 0, 0)); // Trailing LED 1
        }
        if (pos > 1 && strip2Pos + 2 < totalLEDs) {
          safeSetPixelColor(strip2Pos + 2, strip.Color(maxBrightness / 8, 0, 0)); // Trailing LED 2
        }
      }
      
      safeStripShow();
      delay(delayTime);
    }

    // Backward sweep: Strip 1 (right to left), Strip 2 (left to right)
    for (int pos = ledBarNum - 1; pos >= 0; pos--) {
      // Clear all LEDs with boundary check
      for (int i = 0; i < totalLEDs; i++) {
        safeSetPixelColor(i, strip.Color(0, 0, 0));
      }

      // Strip 1 (lower strip): right to left (positions 0 to ledBarNum-1)
      safeSetPixelColor(pos, strip.Color(maxBrightness, 0, 0)); // Main LED
      if (pos < ledBarNum - 1) {
        safeSetPixelColor(pos + 1, strip.Color(maxBrightness / 3, 0, 0)); // Trailing LED 1
      }
      if (pos < ledBarNum - 2) {
        safeSetPixelColor(pos + 2, strip.Color(maxBrightness / 8, 0, 0)); // Trailing LED 2
      }
      
      // Strip 2 (upper strip): left to right (positions ledBarNum to ledBarNum+LedBar2_Num-1)
      int strip2Pos = ledBarNum + (LedBar2_Num - 1 - pos); // Fixed mirror position calculation
      if (strip2Pos >= ledBarNum && strip2Pos < totalLEDs) { // Boundary check
        safeSetPixelColor(strip2Pos, strip.Color(maxBrightness, 0, 0)); // Main LED
        if (pos < ledBarNum - 1 && strip2Pos - 1 >= ledBarNum) {
          safeSetPixelColor(strip2Pos - 1, strip.Color(maxBrightness / 3, 0, 0)); // Trailing LED 1
        }
        if (pos < ledBarNum - 2 && strip2Pos - 2 >= ledBarNum) {
          safeSetPixelColor(strip2Pos - 2, strip.Color(maxBrightness / 8, 0, 0)); // Trailing LED 2
        }
      }
      
      safeStripShow();
      delay(delayTime);
    }
  }

  // Clear all LEDs after animation and ensure both strips are off
  for (int i = 0; i < totalLEDs; i++) {
    safeSetPixelColor(i, strip.Color(0, 0, 0));
  }
  safeStripShow();
  
  // Small delay to ensure the clear is visible
  delay(100);
  
  // Force another clear to be absolutely sure
  for (int i = 0; i < totalLEDs; i++) {
    safeSetPixelColor(i, strip.Color(0, 0, 0));
  }
  safeStripShow();
  
  unlockLedBar();
}
