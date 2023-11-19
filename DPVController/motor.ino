/**
*
* Methods that control motor and speed.
*/

/*
*  CONSTANTS
*/
const int SPEED_STEPS = 10;                  //Number speed steps
const int STANDBY_DELAY_US = 60 /*s*/ * 1000 * 1000;  // Time until the motor goes into standby.
const int BATTERY_POWER_MAX = 40; // in Ampere
const unsigned long MAX_DELTA_US = 30/*microseconds*/ * 1000; //Maximum time from last run to consider for smooth acceleration
const double DUTY_FACTOR = 1.0;
const double MIN_SPEED_DUTY = 0.38; //Duty on lowest setting.
const double MIN_DUTY_SOFT = 0.23; //Minumum Duty we sent to the motor during soft acceleration. 
const int SPEED_UP_TIME_US = 5/*s*/ * 1000 * 1000;    //time we want to take to  speed the motor from 0 to  full power.
const int SPEED_DOWN_TIME_US = 500/*ms*/ * 1000;  //time we want to take to  speed the motor from full power to 0.
const long MAX_TIME_OVERLOADED = 5/*s*/ * 1000; //Maximum time in ms that we overload the battery before lowering motor power.
/*
* GLOBAL VARIABLES 
*/
double currentMotorSpeed = 0;           //Speed the motor is currently running at(0.0-1.0)
int currentMotorStep = 1;//The current speed setting. stays the same, even if motor is turned off. 
//Goes from 1(very slow) to SPEED_STEPS(max)
int overloadSpeedThrottle = NEVER; //Either NEVER or the maximum speed setting before we would overload the battery.
unsigned long currentMotorTime = micros();  //Time in microseconds when we last changed the currentMotorSpeed
double targetMotorSpeed = 0.0;  //The desired motor speed. In Percent of max-power.
double lastTargetMotorSpeed = targetMotorSpeed;
double lastPrintedMotorSpeed = -1;
bool hasMotor = true;
VescUart UART;
VescUart getVescUart(){return UART;}//Accessor
unsigned long overLoadedSince = NEVER; //microsecond timestamp.

void motorSetup(){
  // Initialize VESC UART communication
  Serial1.begin(115200, SERIAL_8N1, VESCRX, VESCTX);
  while (!Serial1) { ; }
  delay(500);
  UART.setSerialPort(&Serial1);
  delay(500);
  if (UART.getVescValues()) {
    Serial.println("Connected to VESC.");
  } else {
    Serial.println("Failed to connect to VESC.");
    //hasMotor = false;
  }
}

void motorLoop(){
  preventOverload();
  controlStandby();
  controlMotor();
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
  if (motorState != standby)  {
    if (lastActionTime + STANDBY_DELAY_US < micros()) {
      standBy();
    }
  }
}

void wakeUp(){
  motorState = off;
  log("leaving standby", 1, true);
  lastActionTime = micros();
  beep("2");
  setBarSpeed(currentMotorStep);
}

void standBy(){
  log("going to standby", micros(), true);
  motorState = standby;
  beep("2");
  setBarStandby();
}

void controlMotor() {
  if (motorState == standby || motorState == off) {
    // Motor is off
    targetMotorSpeed = 0.0;
  } else if (motorState == on || motorState == cruise) {
    targetMotorSpeed = MIN_SPEED_DUTY + ((double)currentMotorStep)/SPEED_STEPS * (1-MIN_SPEED_DUTY);
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
      max(MIN_DUTY_SOFT, 
      //Do not overshoot the actual targetMotorSpeed
      min(currentMotorSpeed, targetMotorSpeed));
  } else if(currentMotorSpeed > targetMotorSpeed) {
    //Speed down
    float maxChange = timePassedSinceLastChange / SPEED_DOWN_TIME_US;
    currentMotorSpeed -= maxChange;
    currentMotorSpeed = max(currentMotorSpeed, targetMotorSpeed);
  }
  if(abs(currentMotorSpeed) > 0.0){
    UART.setDuty(currentMotorSpeed * DUTY_FACTOR);
  }
  currentMotorTime = micros();

  if(EnableDebugLog && abs(currentMotorSpeed - lastPrintedMotorSpeed) >= 0.01){
    Serial.print("currentMotorSpeed: ");
    Serial.println(currentMotorSpeed);
    lastPrintedMotorSpeed = currentMotorSpeed;
  }
}

float getMotorPower(){
  //return 50.0*currentMotorSpeed;//Great for testing with no motor.
  return UART.data.avgInputCurrent;
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

/**
* Calculate how much power in Ampere the motor can
* drain without overloading the battery.
**/
float maxAvailablePowerForMotor(){
  return BATTERY_POWER_MAX - getLedLampPower();
}

void enterCruiseMode(){
  log("Entering cruise mode", 0);
  motorState = cruise;
}

void leaveCruiseMode(){
  log("leaving cruise mode", 0);
  motorState = off;
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
