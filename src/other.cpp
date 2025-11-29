#include "main.h"
#include "constants.h"
#include "beep.h"
#include "log.h"
#include "motor.h"
#include "battery.h"
#include "ledBar.h"
#include "ledLamp.h"
#include <LittleFS.h>  // For persistent leak alarm storage

/**
* For code that I did not find a better place for.
*/


int FromTimeToTimeIntervall = 50;

/**
 * Save persistent leak alarm state to file
 */
void savePersistentLeakAlarm() {
  File file = LittleFS.open("/leak_alarm.txt", "w");
  if (file) {
    file.println(leakAlarmPersistent);
    file.println(leakAlarmFrontPersistent);
    file.println(leakAlarmBackPersistent);
    file.close();
    
    String saveMsg = "Saved persistent leak alarm - Global: " + String(leakAlarmPersistent) + 
                    ", Front: " + String(leakAlarmFrontPersistent) + 
                    ", Back: " + String(leakAlarmBackPersistent);
    log(saveMsg.c_str());
  } else {
    log("ERROR: Failed to save persistent leak alarm");
  }
}

/**
 * Load persistent leak alarm state from file (called at startup)
 */
void loadPersistentLeakAlarm() {
  if (LittleFS.exists("/leak_alarm.txt")) {
    File file = LittleFS.open("/leak_alarm.txt", "r");
    if (file) {
      // Read line by line for multiple values
      if (file.available()) {
        String line1 = file.readStringUntil('\n');
        line1.trim();
        leakAlarmPersistent = line1.toInt();
      }
      if (file.available()) {
        String line2 = file.readStringUntil('\n');
        line2.trim();
        leakAlarmFrontPersistent = line2.toInt();
      }
      if (file.available()) {
        String line3 = file.readStringUntil('\n');
        line3.trim();
        leakAlarmBackPersistent = line3.toInt();
      }
      file.close();
      
      String loadMsg = "Loaded persistent leak alarm - Global: " + String(leakAlarmPersistent) + 
                      ", Front: " + String(leakAlarmFrontPersistent) + 
                      ", Back: " + String(leakAlarmBackPersistent);
      log(loadMsg.c_str());
      
      if (leakAlarmPersistent == 1) {
        log("PERSISTENT LEAK ALARM restored from file - WATER INTRUSION DETECTED!");
      }
    } else {
      log("Failed to open persistent leak alarm file");
    }
  } else {
    leakAlarmPersistent = 0; // Default to no alarm
    leakAlarmFrontPersistent = 0;
    leakAlarmBackPersistent = 0;
    log("No persistent leak alarm file found - starting with clean state");
  }
}

/**
 * Clear persistent leak alarm (for web GUI reset function)
 */
void clearPersistentLeakAlarm() {
  leakAlarmPersistent = 0;
  leakAlarmFrontPersistent = 0;
  leakAlarmBackPersistent = 0;
  savePersistentLeakAlarm();
  log("ALL persistent leak alarms CLEARED by user");
}

/**
 * Debug function to print current leak alarm states
 */
void debugLeakAlarmStates() {
  String debugMsg = "=== LEAK ALARM DEBUG === Current: Front=" + String(leakSensorFront) + ", Back=" + String(leakSensorBack) + 
                   " | Persistent: Front=" + String(leakAlarmFrontPersistent) + ", Back=" + String(leakAlarmBackPersistent) + 
                   ", Global=" + String(leakAlarmPersistent);
  log(debugMsg.c_str());
}


void checkForLeak() {
  // Read current sensor states
  int frontCurrentState = digitalRead(PIN_LEAK_FRONT);
  int backCurrentState = digitalRead(PIN_LEAK_BACK);

  // Update individual sensor states (0=OK, 1=LEAK detected)
  leakSensorFront = (frontCurrentState == LOW) ? 1 : 0;
  leakSensorBack = (backCurrentState == LOW) ? 1 : 0;

  // Trigger persistent alarm for specific sensors
  if (leakSensorFront && leakAlarmFrontPersistent == 0) {
    leakAlarmFrontPersistent = 1;
    leakAlarmPersistent = 1; // Keep global for compatibility
    savePersistentLeakAlarm(); // Save to file for persistence across reboots
    
    String alarmMsg = "PERSISTENT FRONT LEAK ALARM TRIGGERED";
    log(alarmMsg.c_str());
  }
  
  if (leakSensorBack && leakAlarmBackPersistent == 0) {
    leakAlarmBackPersistent = 1;
    leakAlarmPersistent = 1; // Keep global for compatibility
    savePersistentLeakAlarm(); // Save to file for persistence across reboots
    
    String alarmMsg = "PERSISTENT BACK LEAK ALARM TRIGGERED";
    log(alarmMsg.c_str());
  }
  
  // Set LED bar if any leak detected
  if ((leakSensorFront || leakSensorBack) && !isLampSequenceActive()) {
    setBarLeak();
  }
  
  // Debug logging for sensor states (only when changed)
  static int lastFrontState = -1;
  static int lastBackState = -1;
  static int lastFrontPersistent = -1;
  static int lastBackPersistent = -1;
  
  if (leakSensorFront != lastFrontState || leakSensorBack != lastBackState || 
      leakAlarmFrontPersistent != lastFrontPersistent || leakAlarmBackPersistent != lastBackPersistent) {
    String sensorMsg = "Leak sensors - Front: " + String(leakSensorFront) + " (persistent: " + String(leakAlarmFrontPersistent) + "), " +
                      "Back: " + String(leakSensorBack) + " (persistent: " + String(leakAlarmBackPersistent) + "), " +
                      "Global persistent: " + String(leakAlarmPersistent);
    log(sensorMsg.c_str());
    lastFrontState = leakSensorFront;
    lastBackState = leakSensorBack;
    lastFrontPersistent = leakAlarmFrontPersistent;
    lastBackPersistent = leakAlarmBackPersistent;
  }
}

void FromTimeToTimeExecution() {
  if (loopCount % FromTimeToTimeIntervall == 0) {
    // Enhanced leak beep that considers persistent alarm
    if (leakAlarmPersistent == 1) {
      BeepForLeak(); // Keep beeping as long as persistent alarm is active
    }
    BlinkForLongStandby();
    BatteryLevelAlert();
  }
}
