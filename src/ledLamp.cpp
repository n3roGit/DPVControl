/***
*  Code that controls the main lamp.
***/
#include "ledLamp.h"
#include "BlinkSequence.h"
#include "constants.h"
#include "ledBar.h"
#include "log.h"
#include "motor.h"
#include "battery.h" // Include battery header
#include "beep.h"
#include "Arduino.h"
#include "button.h"
#include "settings.h"

/*
*  CONSTANTS
*/

// LED PWM parameters
// Note: LEDfrequency is now read from settings - see getLedFrequency()
const int LEDresolution = 8;   // PWM resolution (8-bit = 0-255 values)
const int LEDchannel = 0;      // PWM channel number (0-15 available)
const int LAMP_OFF = 0;
const int LAMP_MAX = 4;
const int StandbyBlinkStart = 15 * 60/*s*/ * 1000 * 1000;         //in microseconds. 15 Minutes for blink start
const int standbyBlinkInterval = 10*1000*1000;      // microseconds between blink
const int LAMP_BLINK_PAUSE = 400;                        // ms pause between lamp sequence blinks

/*
* VARIABLES 
*/
static int preBlinkLEDState = LAMP_OFF;
static bool shouldRestoreLED = false;
static unsigned long blinkRestoreTime = 0;
static bool isFlashing = false;
static int flashStep = 0;
static unsigned long flashStepTime = 0;
int LED_State = LAMP_OFF;
int lastStandbyBlinkTime = 0; //The timestamp(ms) when we last blinked for standby-warning.
static unsigned long lastStatusChangeTime = 0;
static bool isInStandby = false;
static bool isStatusRestorationPending = false; // New flag to track if restoration is pending
extern int currentMotorStep; // Declare external variable
static volatile int pendingLampLevel = -1; // For cross-core requests

void requestSetLampLevel(int level) {
    pendingLampLevel = level;
}



void setLEDState(int state);


void turnLampOn(){setLEDState(getLampMaxLevels() - 1);}
void turnLampOff() {
  // Turn lamp off for blinking without changing LED_State
  setLEDState(LAMP_OFF);
}

Blinker lampBlinker = Blinker(turnLampOn, turnLampOff);

long lampDuration(char c){
  if (c == '1') return 200;      // Short blink
  if (c == 'F') return 1000;     // Flash
  return 600;                    // Long blink or pause
}

BlinkSequence lampSequence = BlinkSequence(lampBlinker, lampDuration, LAMP_BLINK_PAUSE);

bool isLampSequenceActive() {
  return isStatusRestorationPending || isFlashing || shouldRestoreLED;
}

void ledLampSetup(){
    // Initialize LED PWM
  pinMode(PIN_LAMP, OUTPUT);                            // Set GPIO pin as output for LED control
  ledcSetup(0, getLedFrequency(), LEDresolution);            // Configure PWM channel 0 with frequency and resolution
  ledcAttachPin(PIN_LAMP, 0);                           // Attach pin to PWM channel 0
}

// Function to apply lamp settings changes at runtime
void applyLampSettings() {
  log("Applying lamp settings changes...");
  
  // Reconfigure PWM frequency if it has changed
  int currentFreq = getLedFrequency();
  String freqMsg = "Updating lamp PWM frequency to: " + String(currentFreq) + " Hz";
  log(freqMsg.c_str());
  
  // Reconfigure the PWM channel with new frequency
  ledcSetup(0, currentFreq, LEDresolution);
  
  // Reapply current brightness in case frequency change affected it
  if (LED_State > 0) {
    int brightness = getLampBrightness(LED_State);
    ledcWrite(0, brightness);
    String brightnessMsg = "Reapplied brightness: " + String(brightness) + " for level " + String(LED_State);
    log(brightnessMsg.c_str());
  }
  
  log("Lamp settings applied successfully");
}

void ledLampLoop(){
  // Process pending requests from other cores (e.g. webserver)
  if (pendingLampLevel != -1) {
    int level = pendingLampLevel;
    pendingLampLevel = -1; // Atomic enough for int
    setLampLevel(level);
  }

  lampSequence.loop();
  lampBlinker.loop();
  
  // Handle flash sequence
  if (isFlashing && millis() >= flashStepTime) {
    switch (flashStep) {
      case 1: // Step 1: Turn off for 1 second
        setLEDState(LAMP_OFF);
        flashStepTime = millis() + 1000;
        flashStep = 2;
        break;
      case 2: // Step 2: Flash on max for 1 second  
        setLEDState(getLampMaxLevels() - 1);
        flashStepTime = millis() + 1000;
        flashStep = 3;
        break;
      case 3: // Step 3: Turn off for 1 second
        setLEDState(LAMP_OFF);
        flashStepTime = millis() + 1000;
        flashStep = 4;
        break;
      case 4: // Step 4: Restore original state
        LED_State = preBlinkLEDState;
        setLEDState(LED_State);
        setBarFlasher(false); // Deactivate flasher
        isInStandby = (motorState == standby);
        if (!isStatusRestorationPending) { // Only start timer if not already pending
          lastStatusChangeTime = millis();
          isStatusRestorationPending = true;
        }
        if (isInStandby) {
          setBarStandby(false); // Restore standby display, NO IMMEDIATE SHOW
        } else {
          setBarSpeed(currentMotorStep, false); // Restore speed display, NO IMMEDIATE SHOW
        }
        updateBatteryDisplay(); // Ensure battery display is correct and SHOW all changes
        isFlashing = false;
        flashStep = 0;
        break;
    }
  }
  
  // Restore LED state after blink sequence (for SOS etc.)
  if (shouldRestoreLED && millis() >= blinkRestoreTime) {
    // Restore previous state and update LED and bar
    LED_State = preBlinkLEDState;
    setLEDState(LED_State);
    setBarLED(LED_State);
    shouldRestoreLED = false;
  }

  // Check if 10 seconds have passed since last status change
  if (isStatusRestorationPending && millis() - lastStatusChangeTime >= 10000) {
    isInStandby = (motorState == standby); // Update standby state
    forceRefreshLedBar(); // Force refresh to ensure display update
    if (isInStandby) {
      setBarStandby(false); // Keep standby display, NO IMMEDIATE SHOW
    } else {
      setBarSpeed(currentMotorStep, false); // Restore speed display, NO IMMEDIATE SHOW
    }
    updateBatteryDisplay(); // Ensure battery display is also refreshed (implicitly calls show())
    isStatusRestorationPending = false; // Reset pending flag
  }
}

void flash(){
  if (!isFlashing) { // Only start flash if not already flashing
    log("flash called at", millis(), true); // Add timestamp to debug log
    preBlinkLEDState = LED_State;  // Save current state
    isFlashing = true;
    flashStep = 1;
    flashStepTime = millis(); // Start immediately
    setBarFlasher(true); // Activate flasher on LED bar
  }
}

void setLampLevel(int level) {
  int maxLevels = getLampMaxLevels();
  // Validate level
  if (level < 0) level = 0;
  if (level >= maxLevels) level = maxLevels - 1;
  
  LED_State = level;
  setLEDState(LED_State);
  
  // Handle LED Bar display
  if (LED_State == LAMP_OFF) {
      // Immediate revert to standard display
      forceRefreshLedBar();
      if (motorState == standby) {
          setBarStandby(false); // NO IMMEDIATE SHOW
      } else {
          setBarSpeed(currentMotorStep, false); // NO IMMEDIATE SHOW
      }
      updateBatteryDisplay(); // Ensure battery display is also refreshed (implicitly calls show())
      isStatusRestorationPending = false;
  } else {
      // Show lamp level and start timeout
      setBarLED(LED_State);
      lastStatusChangeTime = millis();
      isStatusRestorationPending = true;
  }
}

void toggleLED(){
  int nextState = LED_State + 1;
  int maxLevel = getLampMaxLevels() - 1; 
  if (nextState > maxLevel) nextState = LAMP_OFF;
  
  setLampLevel(nextState);
  log("LED_State", LED_State, true);
}

void setLEDState(int state) {
  int brightness;
  
  // Get lamp settings from settings system
  int maxLevels = getLampMaxLevels();
  
  // Validate state against current settings
  if (state < 0 || state > maxLevels) {
    // Invalid state, use off
      brightness = 0;
  } else {
    // Use configured brightness from settings
    brightness = getLampBrightness(state);
  }
  /*
  Is it possible to change pwm frequency to advoid led flickering while filming 
  */
  //analogWrite(PIN_LED, brightness);  // LED-PIN, Brightness 0-255
  ledcWrite(0, brightness);  // Set LED brightness using PWM channel 0
}

void blinkLED(const String& sequence) {
  // If LED was on before blinking, save state and turn off
  if (LED_State != LAMP_OFF) {
    preBlinkLEDState = LED_State;
    shouldRestoreLED = true;
    // calculate total time for the blink sequence
    unsigned long totalTime = 0;
    for (unsigned int i = 0; i < sequence.length(); ++i) {
      totalTime += lampDuration(sequence.charAt(i)) + LAMP_BLINK_PAUSE;
    }
    blinkRestoreTime = millis() + totalTime;
    turnLampOff();
  }
  lampSequence.blink(sequence);
}


void BlinkForLongStandby() {
  unsigned long standbyBlinkStartUs = (unsigned long)getStandbyBlinkStart() * 60UL * 1000UL * 1000UL; // Convert minutes to microseconds
  unsigned long standbyBlinkIntervalUs = (unsigned long)getStandbyBlinkDuration() * 1000UL * 1000UL; // Convert seconds to microseconds
  
  if (motorState == standby && micros() - lastActionTime >= standbyBlinkStartUs && micros() - lastStandbyBlinkTime > standbyBlinkIntervalUs) {
    blinkLED("111222111");  // SOS sequence
    beep("111222111");      // Keep this beep for SOS
    log("sos iam alone", 111222111, true);
    lastStandbyBlinkTime = micros();  // Update the time of the last call
  } 
}

/**
* Return current LED brightness as percentage (0-100%)
*/
int getLedBrightnessPercent(){
  if (LED_State <= 0) return 0;
  
  int brightness = getLampBrightness(LED_State);
  
  // Convert PWM value (0-255) to percentage (0-100)
  return (int)((brightness / 255.0) * 100.0);
}

/**
* Return current power consumption in Ampere. 
*/
float getLedLampPower(){
  if (LED_State <= 0) return 0;
  
  int brightness = getLampBrightness(LED_State);
  
  // Estimate power consumption based on brightness (approximate values)
  // These are rough estimates - adjust based on your actual LED specs
  return (brightness / 255.0) * 3.9; // Max power at full brightness
}
