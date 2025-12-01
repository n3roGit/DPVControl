#include "motor.h"
#include "constants.h"
#include "beep.h"
#include "log.h"
#include "ledBar.h"
#include "button.h"
#include "ledLamp.h"
#include "battery.h"
#include "settings.h"
#include "vesc_task.h" // Include VESC task interface

/**
*
* Methods that control motor and speed.
*/

MotorState motorState = standby;
bool remoteControlActive = false; // Flag for remote control override
const bool HAS_MOTOR = true;//Indicates that we have an actual motor plugged in.
bool reverseModeActive = false;    // Reverse drive mode flag

/*
*  CONSTANTS
*/
const int SPEED_STEPS = 10;                  //Number speed steps
const int STANDBY_DELAY_US = 60 /*s*/ * 1000 * 1000;  // Time until the motor goes into standby.
const int BATTERY_POWER_MAX = 40; // in Ampere
const unsigned long MAX_DELTA_US = 30/*microseconds*/ * 1000; //Maximum time from last run to consider for smooth acceleration
const double MIN_SPEED_PERCENT = 0.38; //Speed on lowest setting in percent of max.
const double MIN_SPEED_SOFT = 0.1; //Minumum % we sent to the motor during soft acceleration. 
const double MAX_SPEED_RPM = 15800; //Maximum speed in rpm. Speed of 100% 
const int SPEED_UP_TIME_US = 3/*s*/ * 1000 * 1000;    //time we want to take to  speed the motor from 0 to  full power.
const int SPEED_DOWN_TIME_US = 500/*ms*/ * 1000;  //time we want to take to  speed the motor from full power to 0.
const long MAX_TIME_OVERLOADED = 5/*s*/ * 1000; //Maximum time in ms that we overload the battery before lowering motor power.

const float JAM_MIN = 0.2; //The minumum speed in % for jam detection
const float JAM_DETECTION_THRESHOLD = 0.5; //Percentage of target speed
//that we must be under for a jam to be detected.

/*
* VARIABLES 
*/
double currentMotorSpeed = 0;           //Speed the motor is currently running at(0.0-1.0)
int currentMotorStep = 3;//The current speed setting. stays the same, even if motor is turned off. 
//Goes from 1(very slow) to SPEED_STEPS(max)
int overloadSpeedThrottle = NEVER; //Either NEVER or the maximum speed setting before we would overload the battery.
unsigned long currentMotorTime = micros();  //Time in microseconds when we last changed the currentMotorSpeed
double targetMotorSpeed = 0.0;  //The desired motor speed. In Percent of max-power.
double lastTargetMotorSpeed = targetMotorSpeed;
double lastPrintedMotorSpeed = -1;
unsigned long overLoadedSince = NEVER; //microsecond timestamp.
unsigned long lastStandbyBeepTime = 0;


void motorSetup(){
  // VESC initialization is now handled in vescTask (vesc_task.cpp)
  log("Motor setup complete (VESC handled by task)");
}

void toggleReverseMode(){
  reverseModeActive = !reverseModeActive;
  if (reverseModeActive) {
    log("Reverse mode enabled", 0);
    beep("2"); // Long beep for reverse enabled
  } else {
    log("Reverse mode disabled", 0);
    beep("1"); // Short beep for reverse disabled
  }
  // Force LED bar to refresh to show or hide reverse indication
  forceRefreshLedBar();
  setBarSpeed(currentMotorStep);
}

bool isReverseModeActive(){
  return reverseModeActive;
}

void speedUp(){
  int maxSteps = getSpeedSteps();
  if (currentMotorStep == maxSteps){
    beep("1");
  }else if(overloadSpeedThrottle != NEVER 
    &&currentMotorStep+1>=overloadSpeedThrottle){
    log("No speedup because overloaded.");
    beep("111");
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
    unsigned long standbyDelayUs = (unsigned long)getStandbyDelay() * 1000UL * 1000UL; // Convert seconds to microseconds
    if (lastActionTime + standbyDelayUs < micros()) {
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
  beep("11");
  setBarStandby();
}

/**
* Slowly changes the motor speed to targetMotorSpeed.
*
**/
void setSoftMotorSpeed() {
  float timePassedSinceLastChange = min(micros() - currentMotorTime, MAX_DELTA_US);
  double lastMotorSpeed = currentMotorSpeed;
  
  // Get timing settings from configuration
  unsigned long speedUpTimeUs = (unsigned long)getSpeedUpTime() * 1000UL; // Convert ms to microseconds
  unsigned long speedDownTimeUs = (unsigned long)getSpeedDownTime() * 1000UL; // Convert ms to microseconds
  
  if (currentMotorSpeed < targetMotorSpeed) {
    //Speed up
    float maxChange = timePassedSinceLastChange / (float)speedUpTimeUs;
    currentMotorSpeed += maxChange;
    currentMotorSpeed = 
      //Do not go lower than minimal setting.
      max(MIN_SPEED_SOFT, 
      //Do not overshoot the actual targetMotorSpeed
      min(currentMotorSpeed, targetMotorSpeed));
  } else if(currentMotorSpeed > targetMotorSpeed) {
    //Speed down
    float maxChange = timePassedSinceLastChange / (float)speedDownTimeUs;
    currentMotorSpeed -= maxChange;
    currentMotorSpeed = max(currentMotorSpeed, targetMotorSpeed);
  }
  double effectiveSpeed = currentMotorSpeed * getMaxSpeedRpm();
  // Apply direction based on reverse mode
  if (reverseModeActive) {
    effectiveSpeed = -effectiveSpeed;
  }
  
  // Use the thread-safe function to set target RPM in the VESC task
  setVescTargetRpm(effectiveSpeed);
  
  currentMotorTime = micros();

  if(EnableDebugLog && abs(currentMotorSpeed - lastPrintedMotorSpeed) >= 0.01){
    log("eRPM: " + String(effectiveSpeed, 0) + " (" + String(currentMotorSpeed*100, 0) + "%)");
    lastPrintedMotorSpeed = currentMotorSpeed;
  }
}

void controlMotor() {
  if (motorState == standby || motorState == off || motorState == jammed) {
    // Motor is off
    targetMotorSpeed = 0.0;
  } else if (motorState == on || motorState == cruise || motorState == turbo) {
    float minSpeedPercent = getMinSpeedPercent();
    int speedSteps = getSpeedSteps();
    if (reverseModeActive) {
      // In reverse mode we always run with the first forward speed step
      targetMotorSpeed = minSpeedPercent;
    } else if (motorState == turbo) {
      targetMotorSpeed = 1.0;
    } else {
      targetMotorSpeed = minSpeedPercent + ((double)currentMotorStep-1)/(speedSteps-1) * (1-minSpeedPercent);
    }
  } else{
    log("Unhandled motorstate: " + String(motorState));
  }
  if(EnableDebugLog && abs(lastTargetMotorSpeed - targetMotorSpeed) >= 0.01){
    log("targetMotorSpeed: " + String(targetMotorSpeed));
  }
  setSoftMotorSpeed();
  lastTargetMotorSpeed = targetMotorSpeed;
}


float getMotorPower(){
  // Use thread-safe data access
  return getVescData().avgInputCurrent;
}

/**
* Calculate how much power in Ampere the motor can
* drain without overloading the battery.
**/
float maxAvailablePowerForMotor(){
  return getBatteryPowerMax() - getLedLampPower();
}

void preventOverload(){
  bool overloaded = getMotorPower() >= maxAvailablePowerForMotor();
  if (overloaded){
    if (overLoadedSince == NEVER){
      if (EnableDebugLog){
        log("Overloaded! getMotorPower(): " + String(getMotorPower()));
        log("maxAvailablePowerForMotor(): " + String(maxAvailablePowerForMotor()));
      }
      overLoadedSince = millis();
    }else if(millis() > overLoadedSince + getMaxTimeOverloaded()){
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
  setBarSpeed(currentMotorStep); // Update LED display for cruise mode
}

void leaveCruiseMode(){
  log("leaving cruise mode", 0);
  motorState = off;
  lastActionTime = micros();//Prevent standby right after leaving cruise control
  setBarSpeed(currentMotorStep); // Restore normal LED display
}

void enterTurboMode(){
  log("enter turbo mode", 0);
  motorState = turbo;
  forceRefreshLedBar();
  setBarSpeed(getSpeedSteps());
  updateBatteryDisplay(true);
}

void leaveTurboMode(){
  log("leaving turbo mode", 0);
  motorState = off;
  forceRefreshLedBar();
  setBarSpeed(currentMotorStep);
  updateBatteryDisplay(true);
  lastActionTime = micros();//Prevent standby right after leaving turbo
}

/**
 * Try to detect if there is a jam. If so, shut down the motor. 
*/
void checkJam(){
  // Do not check for jam when running with no motor.
  if(!HAS_MOTOR) return;

  float jamMin = getJamMin();
  float jamThreshold = getJamDetectionThreshold();
  float maxSpeedRpm = getMaxSpeedRpm();
  
  // Get VESC RPM safely
  float currentRpm = getVescData().rpm;
  float rpmEffective = fabs(currentRpm);
  
  if (motorState != jammed && currentMotorSpeed >= jamMin
  && rpmEffective/currentMotorSpeed/maxSpeedRpm < jamThreshold){
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

// Motor control functions
void setMotorSpeed(int speed) {
    // Convert percentage (0-100) to motor steps (0-maxSteps)
    int maxSteps = getSpeedSteps();
    currentMotorStep = (speed * maxSteps) / 100;
    
    // Update motor state
    if (speed > 0) {
        motorState = on;
    } else {
        motorState = off;
    }
    
    // Update LED bar
    setBarSpeed(currentMotorStep);
    
    // Update last action time
    lastActionTime = micros();
}

void updateMotorState() {
    // Check for standby timeout
    if (motorState == on && !remoteControlActive) {
        unsigned long currentTime = micros();
        if (currentTime - lastActionTime > getStandbyDelay() * 1000000) {
            motorState = standby;
            setBarStandby();
        }
    }
}
