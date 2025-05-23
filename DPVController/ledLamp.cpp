/***
*  Code that controls the main lamp.
***/
#include "ledLamp.h"
#include "BlinkSequence.h"
#include "constants.h"
#include "ledBar.h"
#include "log.h"
#include "motor.h"
#include "beep.h"
#include "Arduino.h"
#include "button.h"

/*
*  CONSTANTS
*/

// LED PWM parameters
const int LEDfrequency = 960;  // Initializing the integer variable 'LEDfrequency' as a constant at 4000 Hz. This sets the PWM signal frequency to 4000 Hz.
const int LEDresolution = 8;   // Initializing the integer variable 'LEDresolution' as a constant with 8-bit resolution. This defines the PWM signal resolution as 8 bits.
const int LEDchannel = 0;      // Initializing the integer variable 'LEDchannel' as a constant, set to 0 out of 16 possible channels. This designates the PWM channel as channel 0 out of a total of 16 channels.
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


void setLEDState(int state);


void turnLampOn(){setLEDState(LAMP_MAX);}
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

void ledLampSetup(){
    // Initialize LED PWM
  pinMode(PIN_LAMP, OUTPUT);                            //Setzt den GPIO-Pin 23 als Output (Ausgang)
  ledcSetup(LEDchannel, LEDfrequency, LEDresolution);  //Konfiguriert den PWM-Kanal 0 mit der Frequenz von 1 kHz und einer 8 Bit-Aufloesung
  ledcAttachPin(PIN_LAMP, LEDchannel);                  //Kopplung des GPIO-Pins 23 mit dem PWM-Kanal 0
}

void ledLampLoop(){
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
        setLEDState(LAMP_MAX);
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
          setBarStandby(); // Restore standby display
        } else {
          setBarSpeed(currentMotorStep); // Restore speed display
        }
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
    if (isInStandby) {
      setBarStandby(); // Keep standby display
    } else {
      setBarSpeed(currentMotorStep); // Restore speed display
    }
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

void toggleLED(){
  LED_State++;
  if (LED_State > LAMP_MAX) LED_State = LAMP_OFF;
  setLEDState(LED_State);
  setBarLED(LED_State);
  log("LED_State", LED_State, true);
}

void setLEDState(int state) {

  int brightness;
  switch (state) {
    case LAMP_OFF:
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
    case LAMP_MAX:
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
  if (motorState == standby && micros() - lastActionTime >= StandbyBlinkStart && micros() - lastStandbyBlinkTime > standbyBlinkInterval) {
    blinkLED("111222111");  // SOS sequence
    beep("111222111");      // Keep this beep for SOS
    log("sos iam alone", 111222111, true);
    lastStandbyBlinkTime = micros();  // Update the time of the last call
  } 
}

/**
* Return current power consumption in Ampere. 
*/
float getLedLampPower(){
  switch (LED_State){
    case LAMP_MAX:
      return 3.9;
    case 3:
      return 1.73;
    case 2:
      return 0.8;
    case 1:
      return 0.2;
    default:
      return 0;
  }
}
