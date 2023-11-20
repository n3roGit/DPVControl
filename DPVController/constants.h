#ifndef constants_h
#define constants_h



const int NEVER = -1; //Marker Value for "never" or "nothing".


/*
*  PINS
*/
const int PIN_LEFT_BUTTON = 26;   // GPIO pin for the left button
const int PIN_RIGHT_BUTTON = 27;  // GPIO pin for the right button
const int PIN_LEAK_FRONT = 32;  // GPIO pin for front leak sensor
const int PIN_LEAK_BACK = 33;   // GPIO pin for back leak sensor
const int PIN_LAMP = 25;  // GPIO pin for LED
const int PIN_DHT = 14;  // GPIO pin for the buzzer
const int PIN_BEEP = 18;  //G18 OK
#define VESCRX 22  // GPIO pin for VESC UART RX
#define VESCTX 23  // GPIO pin for VESC UART TX
const int PIN_LEDBAR = 12; // Pin to which the LED strip is connected
const int PIN_POWERBANK = 13; // Pin to which the relay for power bank is connected


#endif