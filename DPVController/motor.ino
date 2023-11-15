/**
*
* Methods that control motor and speed.
*/

/*
*  CONSTANTS
*/
const int SPEED_STEPS = 10;                  //Number speed steps
const int STANDBY_DELAY_US = 60 /*s*/ * 1000 * 1000;  // Time until the motor goes into standby.
const int OverloadLimitMax = 40; // in Ampere
const unsigned long MAX_DELTA_US = 30/*microseconds*/ * 1000; //Maximum time from last run to consider for smooth acceleration
const double DUTY_FACTOR = 1.0;
const double MIN_SPEED_DUTY = 0.30; //Duty on lowest setting.
const double MIN_DUTY_SOFT = 0.23; //Minumum Duty we sent to the motor during soft acceleration. 
const int SPEED_UP_TIME_US = 5/*s*/ * 1000 * 1000;    //time we want to take to  speed the motor from 0 to  full power.
const int SPEED_DOWN_TIME_US = 500/*ms*/ * 1000;  //time we want to take to  speed the motor from full power to 0.

/*
* GLOBAL VARIABLES 
*/
double currentMotorSpeed = 0;           //Speed the motor is currently running at(0.0-1.0)
int currentMotorStep = 1;//The current speed setting. stays the same, even if motor is turned off. Goes from 1(very slow) to SPEED_STEPS(max)
unsigned long currentMotorTime = micros();  //Time in microseconds when we last changed the currentMotorSpeed
double targetMotorSpeed = 0.0;  //The desired motor speed. In Percent of max-power.
double lastTargetMotorSpeed = targetMotorSpeed;
double lastPrintedMotorSpeed = -1;
int OverloadLimit = OverloadLimitMax; // in Ampere
VescUart UART;
VescUart getVescUart(){return UART;}//Accessor

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
  }
}

void motorLoop(){
  controlStandby();
  controlMotor();
}


void speedUp(){
  if (currentMotorStep < SPEED_STEPS){
    currentMotorStep++;
  }
  log("speed up", currentMotorStep, EnableDebugLog);
  setBarSpeed(currentMotorStep);
}

void speedDown(){
  if (currentMotorStep > 1){
    currentMotorStep--;
  }
  log("speed down", currentMotorStep, EnableDebugLog);
  setBarSpeed(currentMotorStep);
}


// Function to control standby mode
void controlStandby() {
  if (motorState != MOTOR_STANDBY)  {
    if (lastActionTime + STANDBY_DELAY_US < micros()) {
      standBy();
    }
  }
}

void wakeUp(){
  motorState = MOTOR_OFF;
  log("leaving standby", 1, true);
  lastActionTime = micros();
  beep("2");
  setBarSpeed(currentMotorStep);
}

void standBy(){
  log("going to standby", micros(), true);
  motorState = MOTOR_STANDBY;
  beep("2");
  setBarStandby();
}

void controlMotor() {
  //log("motorstate", motorState, EnableDebugLog);

  if (motorState == MOTOR_STANDBY || motorState == MOTOR_OFF) {
    // Motor is off
    targetMotorSpeed = 0.0;
  } else if (motorState == MOTOR_ON) {
    targetMotorSpeed = MIN_SPEED_DUTY + ((double)currentMotorStep)/SPEED_STEPS * (1-MIN_SPEED_DUTY);
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


/*
is this a clever solution to prevent overload?
*/
void PreventOverload() {

  if (LED_State == 3) {
    OverloadLimit = OverloadLimitMax - 3;
  } else if (LED_State == 4) {
    OverloadLimit = OverloadLimitMax - 4;
  } else {
    OverloadLimit = OverloadLimitMax;
  }

/*
// something like this to prevent overload. so i can limit the motor speed if i have other devices consuming current
  if (UART.data.avgInputCurrent >= OverloadLimit) {
    speedSetting -= MOTOR_SPEED_CHANGE;
  }  else if (UART.data.avgInputCurrent < OverloadLimit) {
    speedSetting += MOTOR_SPEED_CHANGE;
  }
  */
}