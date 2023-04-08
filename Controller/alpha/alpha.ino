
#include <ESP8266VESC.h>
#include <SoftwareSerial.h>

SoftwareSerial softwareSerial = SoftwareSerial(14, 12); // ESP8266 (NodeMCU); RX (D5), TX (D6 / GPIO12)
ESP8266VESC esp8266VESC = ESP8266VESC(softwareSerial);

const int leftButton = 4;//19; //D2
const int rightButton = 5; //D1

const int MOTOR_OFF = 0;
const int MOTOR_ON = 1;
const int MOTOR_STANDBY = 2;


int buttonState = 0;

void setup() {
  pinMode(leftButton, INPUT);
  pinMode(rightButton, INPUT);

  Serial.begin(9600);

  delay(500);

  // Setup serial connection to VESC
  softwareSerial.begin(115200);
  delay(500);

}

void loop() {
  // buttonState = digitalRead(leftButton);
  // Serial.print("left: ");
  // Serial.print(buttonState);

  // buttonState = digitalRead(rightButton);
  // Serial.print(" right: ");
  // Serial.print(buttonState);

  // Serial.print(" millis: ");
  // Serial.print(millis());

  // Serial.println();
  delay(100);

   Serial.println("Setting duty cycle to -1.0");
  esp8266VESC.setDutyCycle(-1.0f);
  delay(2000);

  Serial.println("Setting duty cycle to -0.5");
  esp8266VESC.setDutyCycle(-0.5f);
  delay(2000);

  Serial.println("Setting duty cycle to 0.0");
  esp8266VESC.setDutyCycle(0.0f);
  delay(2000);

  Serial.println("Setting duty cycle to 0.5");
  esp8266VESC.setDutyCycle(0.5f);
  delay(2000);

  Serial.println("Setting duty cycle to 1.0");
  esp8266VESC.setDutyCycle(1.0f);
  delay(2000);

  Serial.println("Setting duty cycle to 0.0");
  esp8266VESC.setDutyCycle(0.0f);
  delay(2000);

  Serial.println();
  delay(1000);


}
