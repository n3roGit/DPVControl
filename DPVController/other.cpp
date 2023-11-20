#include "all.h"
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


const int VESC_VALUES_INTERVAL = 100;


int FromTimeToTimeIntervall = 500;


/*
only output. needs to be stored in database
*/
void GetVESCValues() {

  if (!hasMotor) return;

  if (getVescUart().getVescValues()) {
    /*
    Serial.print("RPM: ");
    Serial.println(UART.data.rpm);
    Serial.print("inpVoltage: ");
    Serial.println(UART.data.inpVoltage);
    Serial.print("ampHours: ");
    Serial.println(UART.data.ampHours);
    Serial.print("tachometerAbs: ");
    Serial.println(UART.data.tachometerAbs);
    */

    updateBatteryLevel(getVescUart().data.inpVoltage);
  } else {
    log("Failed to get VESC data!", 00000);
  }

}
void checkForLeak() {
  int frontLeakState = digitalRead(PIN_LEAK_FRONT);
  int backLeakState = digitalRead(PIN_LEAK_BACK);

  // Check whether one of the pins is "HIGH"
  if (frontLeakState == LOW || backLeakState == LOW) {
    leakSensorState = 1;  // There is a leak
    log("leakSensorState", leakSensorState, true);
    setBarLeak();    

  }
}


void FromTimeToTimeExecution() {
  if (loopCount % FromTimeToTimeIntervall == 0) {
  BeepForLeak();
  BeepForStandby();
  BlinkForLongStandby();
  BatteryLevelAlert();
  }
}