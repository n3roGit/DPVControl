/*
*   LIBRARIES
*/
#include "ClickButton.h"  //https://github.com/marcobrianza/ClickButton (v1.1.0)
#include "DHTesp.h"  //https://github.com/beegee-tokyo/DHTesp (v1.19)
#include <HardwareSerial.h>
#include <VescUart.h>  //https://github.com/SolidGeek/VescUart (v1.0.1)
#include "uptime_formatter.h"  //https://github.com/YiannisBourkelis/Uptime-Library (v1.0.0)
#include <Adafruit_NeoPixel.h>  //https://github.com/adafruit/Adafruit_NeoPixel (v1.12.0)

#include "FS.h" //Provided by framework (v3.2.0)
#include "SPIFFS.h"//Provided by framework (v3.2.0)

// Webserver libraries
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>

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

