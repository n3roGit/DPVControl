#include "motor.h"
#include "constants.h"
#include "beep.h"
#include "log.h"
#include "ledBar.h"
#include "button.h"
#include "ledLamp.h"

/**
*
* Methods that control motor and speed.
*/

MotorState motorState = standby;
bool hasMotor = true;//Indicates that we have an actual motor plugged in.

VescUart UART;

VescUart& getVescUart(){return UART;}


/*
*  CONSTANTS
*/
const int SPEED_STEPS = 10;                  //Number speed steps
const int STANDBY_DELAY_US = 60 /*s*/ * 1000 * 1000;  // Time until the motor goes into standby.
const int BATTERY_POWER_MAX = 40; // in Ampere
const unsigned long MAX_DELTA_US = 30/*microseconds*/ * 1000; //Maximum time from last run to consider for smooth acceleration
const double MIN_SPEED_PERCENT = 0.38; //Speed on lowest setting in percent of max.
const double MIN_SPEED_SOFT = 0.1; //Minumum % we sent to the motor during soft acceleration. 
const double MAX_SPEED_RPM = 14500; //Maximum speed in rpm. Speed of 100% 
const int SPEED_UP_TIME_US = 5/*s*/ * 1000 * 1000;    //time we want to take to  speed the motor from 0 to  full power.
const int SPEED_DOWN_TIME_US = 500/*ms*/ * 1000;  //time we want to take to  speed the motor from full power to 0.
const long MAX_TIME_OVERLOADED = 5/*s*/ * 1000; //Maximum time in ms that we overload the battery before lowering motor power.

const float JAM_MIN = 0.2; //The minumum speed in % for jam detection
const float JAM_DETECTION_THRESHOLD = 0.5; //Percentage of target speed
//that we must be under for a jam to be detected.

/*
* VARIABLES 
*/
double currentMotorSpeed = 0;           //Speed the motor is currently running at(0.0-1.0)
int currentMotorStep = 1;//The current speed setting. stays the same, even if motor is turned off. 
//Goes from 1(very slow) to SPEED_STEPS(max)
int overloadSpeedThrottle = NEVER; //Either NEVER or the maximum speed setting before we would overload the battery.
unsigned long currentMotorTime = micros();  //Time in microseconds when we last changed the currentMotorSpeed
double targetMotorSpeed = 0.0;  //The desired motor speed. In Percent of max-power.
double lastTargetMotorSpeed = targetMotorSpeed;
double lastPrintedMotorSpeed = -1;
unsigned long overLoadedSince = NEVER; //microsecond timestamp.
unsigned long lastStandbyBeepTime = 0;


void motorSetup(){
  // Initialize VESC UART communication
  Serial1.begin(115200, SERIAL_8N1, VESCRX, VESCTX);
  while (!Serial1) { ; }
  delay(500);
  getVescUart().setSerialPort(&Serial1);
  delay(500);
  if (getVescUart().getVescValues()) {
    Serial.println("Connected to VESC.");
  } else {
    Serial.println("Failed to connect to VESC.");
    hasMotor = false;
  }
}

void speedUp(){
  if (currentMotorStep == SPEED_STEPS){
    beep("1");
  }else if(overloadSpeedThrottle != NEVER 
    &&currentMotorStep+1>=overloadSpeedThrottle){
    log("No speedup because overloaded.");
    beep("11");
  }else{
    log("speed up", currentMotorStep);
    currentMotorStep++;
  }
  setBarSpeed(currentMotorStep);
}

void speedDown(){
  if (currentMotorStep > 1){
    currentMotorStep--;
  }else{
    beep("1");
  }
  log("speed down", currentMotorStep);
  setBarSpeed(currentMotorStep);
}

// Function to control standby mode
void controlStandby() {
  if (motorState == off)  {
    if (lastActionTime + STANDBY_DELAY_US < micros()) {
      standBy();
    }
  }
  
  if (motorState == standby && micros() - lastStandbyBeepTime >= (1 * 60 * 1000000)) {
    beep("1"); 
    log("still in standby");
    lastStandbyBeepTime = micros(); 
  }
}

void wakeUp(){
  motorState = off;
  lastActionTime = micros();
  log("leaving standby");
  beep("2");
  setBarSpeed(currentMotorStep);
}

void standBy(){
  log("going to standby");
  motorState = standby;
  lastStandbyBeepTime = micros();//Avoid the regular beep to be triggered just hwen going to standby
  beep("2");
  setBarStandby();
}

/**
* Slowly changes the motor speed to targetMotorSpeed.
*
**/
void setSoftMotorSpeed() {
  float timePassedSinceLastChange = min(micros() - currentMotorTime, MAX_DELTA_US);
  double lastMotorSpeed = currentMotorSpeed;
  if (currentMotorSpeed < targetMotorSpeed) {
    //Speed up
    float maxChange = timePassedSinceLastChange/ SPEED_UP_TIME_US;
    currentMotorSpeed += maxChange;
    currentMotorSpeed = 
      //Do not go lower than minimal setting.
      max(MIN_SPEED_SOFT, 
      //Do not overshoot the actual targetMotorSpeed
      min(currentMotorSpeed, targetMotorSpeed));
  } else if(currentMotorSpeed > targetMotorSpeed) {
    //Speed down
    float maxChange = timePassedSinceLastChange / SPEED_DOWN_TIME_US;
    currentMotorSpeed -= maxChange;
    currentMotorSpeed = max(currentMotorSpeed, targetMotorSpeed);
  }
  double effectiveSpeed = currentMotorSpeed * MAX_SPEED_RPM;
  if(abs(effectiveSpeed) > 0.0){
    getVescUart().setRPM(effectiveSpeed);
  }
  currentMotorTime = micros();

  if(EnableDebugLog && abs(currentMotorSpeed - lastPrintedMotorSpeed) >= 0.01){
    Serial.printf("%5.0f RPM (%2.0f%%)",effectiveSpeed, currentMotorSpeed*100);
    Serial.println();
    lastPrintedMotorSpeed = currentMotorSpeed;
  }
}

void controlMotor() {
  if (motorState == standby || motorState == off || motorState == jammed) {
    // Motor is off
    targetMotorSpeed = 0.0;
  } else if (motorState == on || motorState == cruise) {
    targetMotorSpeed = MIN_SPEED_PERCENT + ((double)currentMotorStep-1)/(SPEED_STEPS-1) * (1-MIN_SPEED_PERCENT);
  } else if (motorState == turbo) {
    targetMotorSpeed = 1.0;
  } else{
    Serial.print("Unhandled motorstate: ");
    Serial.println(motorState);
  }
  if(EnableDebugLog && abs(lastTargetMotorSpeed - targetMotorSpeed) >= 0.01){
    Serial.print("targetMotorSpeed: ");
    Serial.println(targetMotorSpeed);
  }
  setSoftMotorSpeed();
  lastTargetMotorSpeed = targetMotorSpeed;
}


float getMotorPower(){
  //return 50.0*currentMotorSpeed;//Great for testing with no motor.
  return getVescUart().data.avgInputCurrent;
}

/**
* Calculate how much power in Ampere the motor can
* drain without overloading the battery.
**/
float maxAvailablePowerForMotor(){
  return BATTERY_POWER_MAX - getLedLampPower();
}

void preventOverload(){
  bool overloaded = getMotorPower() >= maxAvailablePowerForMotor();
  if (overloaded){
    if (overLoadedSince == NEVER){
      if (EnableDebugLog){
        Serial.print("Overloaded! getMotorPower(): ");
        Serial.println(getMotorPower());
        Serial.print("maxAvailablePowerForMotor(): ");
        Serial.println(maxAvailablePowerForMotor());        
      }
      overLoadedSince = millis();
    }else if(millis() > overLoadedSince + MAX_TIME_OVERLOADED){
      log("Overloaded for too long. Lowering speed.");
      beep("12");
      speedDown();
      overloadSpeedThrottle = currentMotorStep;
      overLoadedSince = millis();//causes us to wait again
      //and if necessary, reduce speed again.
    }
  }else if(overLoadedSince != NEVER){
    overLoadedSince = NEVER;
    overloadSpeedThrottle = NEVER;
    beep("21");
    log("No longer overloaded", 0);
  }
}

void enterCruiseMode(){
  log("Entering cruise mode", 0);
  motorState = cruise;
}

void leaveCruiseMode(){
  log("leaving cruise mode", 0);
  motorState = off;
  lastActionTime = micros();//Prevent standby right after leaving cruise control
}

void enterTurboMode(){
  log("enter turbo mode", 0);
  setBarSpeed(SPEED_STEPS);
  motorState = turbo;
}

void leaveTurboMode(){
  log("leaving turbo mode", 0);
  setBarSpeed(currentMotorStep);
  motorState = off;
}

/**
 * Try to detect if there is a jam. If so, shut down the motor. 
*/
void checkJam(){
  // Do not check for jam when running with no motor.
  if(!hasMotor) return;

  if (motorState != jammed && currentMotorSpeed >= JAM_MIN
  && getVescUart().data.rpm/currentMotorSpeed/MAX_SPEED_RPM < JAM_DETECTION_THRESHOLD){
    log("MOTOR JAMMED!");
    beep("211");
    motorState = jammed;
  }
  if (motorState == jammed && currentMotorSpeed < 0.0001 ){
    log("motor stopped after being jammed. going standby.");
    standBy();  
  }
}

void motorLoop(){
  preventOverload();
  checkJam();
  controlStandby();
  controlMotor();
}