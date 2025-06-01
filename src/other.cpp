#include "main.h"
#include "constants.h"
#include "beep.h"
#include "log.h"
#include "motor.h"
#include "battery.h"
#include "ledBar.h"
#include "ledLamp.h"

/**
* For code that I did not find a better place for.
*/


const int VESC_VALUES_INTERVAL = 100;  // Update interval in milliseconds
static unsigned long lastVescUpdate = 0;  // Track last VESC data update time


int FromTimeToTimeIntervall = 50;


/*
only output. needs to be stored in database
*/
void GetVESCValues() {

  if (!HAS_MOTOR) return;

  // Implement the VESC_VALUES_INTERVAL timing to prevent UART overload
  unsigned long currentTime = millis();
  if (currentTime - lastVescUpdate < VESC_VALUES_INTERVAL) {
    return; // Skip update if interval hasn't passed
  }

  if (getVescUart().getVescValues()) {
    updateBatteryLevel(getVescUart().data.inpVoltage);
    lastVescUpdate = currentTime; // Update timestamp only on successful read
  } else {
    log("Failed to get VESC data!", 00000);
    // Don't update lastVescUpdate on failure, allowing retry sooner
  }

}
void checkForLeak() {
  int frontLeakState = digitalRead(PIN_LEAK_FRONT);
  int backLeakState = digitalRead(PIN_LEAK_BACK);

  // Check whether one of the pins is "LOW"
  if (frontLeakState == LOW || backLeakState == LOW) {
    leakSensorState = 1;  // There is a leak
    log("leakSensorState", leakSensorState, true);
    setBarLeak();    
  }
}


void FromTimeToTimeExecution() {
  if (loopCount % FromTimeToTimeIntervall == 0) {
  BeepForLeak();
  BlinkForLongStandby();
  BatteryLevelAlert();
  }
}