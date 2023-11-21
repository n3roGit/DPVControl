/*
*   LIBRARIES
*/
#include "ClickButton.h"  //https://github.com/marcobrianza/ClickButton
#include "DHTesp.h"  //https://github.com/beegee-tokyo/DHTesp
#include <HardwareSerial.h>
#include <VescUart.h>  //https://github.com/SolidGeek/VescUart
#include "uptime_formatter.h"  //https://github.com/YiannisBourkelis/Uptime-Library
#include <Adafruit_NeoPixel.h>  //https://github.com/adafruit/Adafruit_NeoPixel

#include "FS.h" //Provided by framework
#include "SPIFFS.h"//Provided by framework


#include "main.h"


/*
*  CONSTANTS
*/

//Stuff below should be moved
unsigned long lastBlinkTime = 0;    
unsigned long buttonPressStartTime = 0;
unsigned long leftButtonDownTime = 0;
unsigned long rightButtonDownTime = 0;
int FromTimeToTime = 0;

