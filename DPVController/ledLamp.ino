/***
*  Code that controls the main lamp.
***/

/*
*  CONSTANTS
*/

// LED PWM parameters
const int LEDfrequency = 960;  // Initializing the integer variable 'LEDfrequency' as a constant at 4000 Hz. This sets the PWM signal frequency to 4000 Hz.
const int LEDresolution = 8;   // Initializing the integer variable 'LEDresolution' as a constant with 8-bit resolution. This defines the PWM signal resolution as 8 bits.
const int LEDchannel = 0;      // Initializing the integer variable 'LEDchannel' as a constant, set to 0 out of 16 possible channels. This designates the PWM channel as channel 0 out of a total of 16 channels.

/*
* GLOBAL VARIABLES 
*/
void turnLampOn(){setLEDState(LAMP_MAX);}
void turnLampOff(){
  setLEDState(LED_State);//Use previous
}

Blinker lampBlinker = Blinker(turnLampOn, turnLampOff);

void ledLampSetup(){
    // Initialize LED PWM
  pinMode(PIN_LAMP, OUTPUT);                            //Setzt den GPIO-Pin 23 als Output (Ausgang)
  ledcSetup(LEDchannel, LEDfrequency, LEDresolution);  //Konfiguriert den PWM-Kanal 0 mit der Frequenz von 1 kHz und einer 8 Bit-Aufloesung
  ledcAttachPin(PIN_LAMP, LEDchannel);                  //Kopplung des GPIO-Pins 23 mit dem PWM-Kanal 0
}

void ledLampLoop(){
  lampBlinker.loop();
}

void flash(){
  lampBlinker.blink(300);
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


/*
Its working but while blinking the esp doesnt response
*/
void blinkLED(const String& sequence) {
  for (char c : sequence) {
    int blinkDuration = (c == '1') ? 200 : 600;
    lampBlinker.blink(blinkDuration);
    lastBlinkTime = micros();
    while (micros() - lastBlinkTime < 400000) {
      // Pause between blinking
    }
  }
  //todo: after blink set led to the last state of LED_State after 5 seconds
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
