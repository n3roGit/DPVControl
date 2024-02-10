#include "web.h"

//I used 
// https://github.com/espressif/arduino-esp32/blob/master/libraries/WebServer/examples/AdvancedWebServer/AdvancedWebServer.ino
// as a basis.

#include <WiFi.h> //https://github.com/arduino-libraries/WiFi
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "Arduino.h" //For String

#include "datalog.h"

const char *ssid = "Aquazepp";
const char *password = "Aquazepp";

WebServer server(80);

void handleRoot() {
  char temp[400];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  snprintf(temp, 400,

           "<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>ESP32 Demo</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Open Source Diver Propulsion Vehicle</h1>\
    <p>Uptime: %02d:%02d:%02d</p>\
    <p><a href=\"/logs\">View logs</a></p>\
  </body>\
</html>",

           hr, min % 60, sec % 60
          );
  server.send(200, "text/html", temp);
}

void handleListLogs(){
  server.send(200, "text/html", createLogfilesHtml());
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
}


void webSetup(){

    // Connect to Wi-Fi network with SSID and password
  Serial.print("Setting AP (Access Point)…");
  // Remove the password parameter, if you want the AP (Access Point) to be open
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  
  server.begin();

  server.on("/", handleRoot);
  server.on("/logs", handleListLogs);
  server.on("/inline", []() {
    server.send(200, "text/plain", "this works as well");
  });
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
}

void webLoop(){
  server.handleClient();
}