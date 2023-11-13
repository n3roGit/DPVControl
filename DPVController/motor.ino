/**
*
* Methods that control motor and speed.
*/

/*
*  CONSTANTS
*/
const int MOTOR_MAX_SPEED = 14500;
const int MOTOR_MIN_SPEED = 6000;
const int SPEED_UP_TIME_MS = 5000 * 1000;    //time we want to take to  speed the motor from 0 to  full power.
const int SPEED_DOWN_TIME_MS = 300 * 1000;  //time we want to take to  speed the motor from full power to 0.
const int SPEED_STEPS = 10;                  //Number speed steps
const int MOTOR_SPEED_CHANGE = MOTOR_MAX_SPEED / SPEED_STEPS;
const int STANDBY_DELAY_MS = 60 /*s*/ * 1000 * 1000;  // Time until the motor goes into standby.
const int OverloadLimitMax = 40; // in Ampere


/*
* GLOBAL VARIABLES 
*/
int currentMotorSpeed = 0;           //Speed the motor is currently running at
int currentMotorStep = 1;
unsigned long currentMotorTime = 0;  //Time in MS when we last changed the currentMotorSpeed
int speedSetting = MOTOR_MIN_SPEED;  //The current speed setting. stays the same, even if motor is turned off.
int targetMotorSpeed = 0;  //The desired motor speed
int OverloadLimit = OverloadLimitMax; // in Ampere
VescUart UART;

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
  speedSetting += MOTOR_SPEED_CHANGE;
  if (speedSetting > MOTOR_MAX_SPEED) {
    speedSetting = MOTOR_MAX_SPEED;
  }
  log("speed up", speedSetting, EnableDebugLog);
  currentMotorStep = (currentMotorStep < 10) ? currentMotorStep + 1 : 10;
  setBarSpeed(currentMotorStep);
}

void speedDown(){
  speedSetting -= MOTOR_SPEED_CHANGE;
  if (speedSetting < MOTOR_MIN_SPEED) {
    speedSetting = MOTOR_MIN_SPEED;
  }
  log("speed down", speedSetting, EnableDebugLog);
  currentMotorStep = (currentMotorStep > 1) ? currentMotorStep - 1 : 1;
  setBarSpeed(currentMotorStep);
}


// Function to control standby mode
void controlStandby() {
  if (motorState != MOTOR_STANDBY)  {
    if (lastActionTime + STANDBY_DELAY_MS < micros()) {
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
    targetMotorSpeed = 0;
  } else if (motorState == MOTOR_ON) {
    targetMotorSpeed = speedSetting;
  }
  // Serial.print(" targetMotorSpeed: ");
  // Serial.print(targetMotorSpeed);

  setSoftMotorSpeed();
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



/**
* Slowly changes the motor speed to targetMotorSpeed.
*
**/
void setSoftMotorSpeed() {

  float timePassedSinceLastChange = micros() - currentMotorTime;
  bool speedUp = currentMotorSpeed < targetMotorSpeed;

  if (speedUp) {
    float maxChange = timePassedSinceLastChange * MOTOR_MAX_SPEED / SPEED_UP_TIME_MS;
    currentMotorSpeed += maxChange;
    currentMotorSpeed = min(currentMotorSpeed, targetMotorSpeed);
  } else {
    float maxChange = timePassedSinceLastChange * MOTOR_MAX_SPEED / SPEED_DOWN_TIME_MS;
    currentMotorSpeed -= maxChange;
    currentMotorSpeed = max(currentMotorSpeed, targetMotorSpeed);
  }
  //log("currentMotorSpeed", currentMotorSpeed, EnableDebugLog);
  if(currentMotorSpeed != 0){
    UART.setRPM(currentMotorSpeed);
  }
  
  currentMotorTime = micros();
}
