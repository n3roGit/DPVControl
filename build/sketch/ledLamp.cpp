#line 1 "C:\\Users\\christoph.bubeck\\Documents\\GitHub\\DPVControl\\DPVController\\ledLamp.cpp"
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

/*
*  CONSTANTS
*/

// LED PWM parameters
const int LEDfrequency = 960;  // Initializing the integer variable 'LEDfrequency' as a constant at 4000 Hz. This sets the PWM signal frequency to 4000 Hz.
const int LEDresolution = 8;   // Initializing the integer variable 'LEDresolution' as a constant with 8-bit resolution. This defines the PWM signal resolution as 8 bits.
const int LEDchannel = 0;      // Initializing the integer variable 'LEDchannel' as a constant, set to 0 out of 16 possible channels. This designates the PWM channel as channel 0 out of a total of 16 channels.
const int LAMP_OFF = 0;
const int LAMP_MAX = 4;
const int StandbyBlinkStart = 15;         // Minutes for blink start
const int StandbyBlinkDuration = 10;      // Seconds between blink

/*
* VARIABLES 
*/
int LED_State = LAMP_OFF;
unsigned long lastStandbyBlinkTime = 0;
unsigned long StandbyBlinkWarningtime = (StandbyBlinkStart * 60 * 1000000);

void setLEDState(int state);


void turnLampOn(){setLEDState(LAMP_MAX);}
void turnLampOff(){
  setLEDState(LED_State);//Use previous
}

Blinker lampBlinker = Blinker(turnLampOn, turnLampOff);

long lampDuration(char c){
  return (c == '1') ? 200 : 600;
}

BlinkSequence lampSequence = BlinkSequence(lampBlinker, lampDuration, 400);

void ledLampSetup(){
    // Initialize LED PWM
  pinMode(PIN_LAMP, OUTPUT);                            //Setzt den GPIO-Pin 23 als Output (Ausgang)
  ledcSetup(LEDchannel, LEDfrequency, LEDresolution);  //Konfiguriert den PWM-Kanal 0 mit der Frequenz von 1 kHz und einer 8 Bit-Aufloesung
  ledcAttachPin(PIN_LAMP, LEDchannel);                  //Kopplung des GPIO-Pins 23 mit dem PWM-Kanal 0
}

void ledLampLoop(){
  lampSequence.loop();
  lampBlinker.loop();
}

void flash(){
  lampBlinker.blink(500);
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
  lampSequence.blink(sequence);
}


void BlinkForLongStandby() {
  if (motorState == standby && micros() - lastStandbyBlinkTime >= StandbyBlinkWarningtime && LED_State == 0) {
    blinkLED("111222111");  // Hier die gewünschte Sequenz für den Ton
    beep("111222111");
    log("sos iam alone", 111222111, true);
    StandbyBlinkWarningtime = (StandbyBlinkDuration * 1000000);  //every 30 sec
    lastStandbyBlinkTime = micros();                             // Update the time of the last call
  } else {
    StandbyBlinkWarningtime = (StandbyBlinkStart * 60 * 1000000);  //every 3 minutes
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
