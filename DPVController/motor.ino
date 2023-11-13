/**
*
* Methods that control motor and speed.
*/

/*
*  CONSTANTS
*/
const int SPEED_STEPS = 10;                  //Number speed steps
const int STANDBY_DELAY_MS = 60 /*s*/ * 1000 * 1000;  // Time until the motor goes into standby.
const int OverloadLimitMax = 40; // in Ampere
const double DUTY_FACTOR = 1.0;
const double MIN_DUTY = 0.25; //Duty on lowest setting.

/*
* GLOBAL VARIABLES 
*/
int currentMotorStep = 1;//The current speed setting. stays the same, even if motor is turned off. Goes from 1(very slow) to SPEED_STEPS(max)
double targetMotorSpeed = 0.0;  //The desired motor speed. In Percent of max-power.
double lastTargetMotorSpeed = targetMotorSpeed;
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
    targetMotorSpeed = 0.0;
  } else if (motorState == MOTOR_ON) {
    targetMotorSpeed = MIN_DUTY + ((double)currentMotorStep)/SPEED_STEPS * (1-MIN_DUTY);
  }

  if (abs(lastTargetMotorSpeed - targetMotorSpeed) > 0.0001){
    if(EnableDebugLog){
      Serial.print("targetMotorSpeed: ");
      Serial.println(targetMotorSpeed);
    }
  }
  UART.setDuty(targetMotorSpeed * DUTY_FACTOR);
  lastTargetMotorSpeed = targetMotorSpeed;
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