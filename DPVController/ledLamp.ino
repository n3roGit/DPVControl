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
int LED_State = 0;
int LED_State_Last = 0;

void ledLampSetup(){
    // Initialize LED PWM
  pinMode(PIN_LAMP, OUTPUT);                            //Setzt den GPIO-Pin 23 als Output (Ausgang)
  ledcSetup(LEDchannel, LEDfrequency, LEDresolution);  //Konfiguriert den PWM-Kanal 0 mit der Frequenz von 1 kHz und einer 8 Bit-Aufloesung
  ledcAttachPin(PIN_LAMP, LEDchannel);                  //Kopplung des GPIO-Pins 23 mit dem PWM-Kanal 0
}

void toggleLED(){
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
