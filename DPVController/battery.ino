/**
*
* Read the state of the main-battery
*
*/
void outputBatteryInfo(){
  log("battery info", batteryLevel, EnableDebugLog);
  if (batteryLevel < 10) {
    beep("1");
  } else {
    // Determine how many full 10% steps have been reached
    int steps = (batteryLevel +5) / 10;
    steps = constrain(steps, 0, 10 - 1);

    // Generate a string with '1' for each full 10% step
    String beepSequence = "";
    for (int i = 0; i < steps; i++) {
      beepSequence += '2';
    }

    // If steps are present, call up the beep function
    if (steps > 0) {
      beep(beepSequence);
    }
  }
}

void BatteryLevelAlert() {
  if (batteryLevel <= 30 && batteryLevel >= 21 && batteryAlerted != 30) {
    beep("222");  // Three long beeps at 30%
    log("BatteryAlert", batteryLevel, true);
    batteryAlerted = 30;  // Sets the status to 30%

  } else if (batteryLevel <= 20 && batteryLevel >= 11 && batteryAlerted != 20) {
    beep("22");  // Beep twice at 20%
    log("BatteryAlert", batteryLevel, true);
    batteryAlerted = 20;  // Sets the status to 20%

  } else if (batteryLevel <= 10 && batteryAlerted != 10) {
    beep("2");  // One beep at 10%
    log("BatteryAlert", batteryLevel, true);
    batteryAlerted = 10;  // Sets the status to 10%

  }
}


// make a map function for this mapiopenigrecord
void updateBatteryLevel(float voltage) {
  if (voltage >= 5 && CellsInSeries >= 2) {
    float singleCellVoltages[] = {4.18, 4.1, 3.99, 3.85, 3.77, 3.58, 3.42, 3.33, 3.21, 3.00, 2.87};
    int singleCellPercentages[] = {100, 96, 82, 68, 58, 34, 20, 14, 8, 2, 0};

    for (int i = 0; i < sizeof(singleCellVoltages) / sizeof(singleCellVoltages[0]); i++) {
      if (voltage >= singleCellVoltages[i] * CellsInSeries) {
        // Interpolation
        if (i > 0) {
          float voltageRange = singleCellVoltages[i] * CellsInSeries - singleCellVoltages[i - 1] * CellsInSeries;
          int percentageRange = singleCellPercentages[i - 1] - singleCellPercentages[i];
          float voltageDifference = singleCellVoltages[i] * CellsInSeries - voltage;
          float interpolationFactor = voltageDifference / voltageRange;
          batteryLevel = singleCellPercentages[i] + interpolationFactor * percentageRange;
        } else {
          batteryLevel = singleCellPercentages[i];
        }
        break;
      }
    }
    // Ensure that the battery level is limited to the range [0, 100]
    batteryLevel = constrain(batteryLevel, 0, 100);
  } else {
    batteryLevel = 100;
  }
  int steps = (batteryLevel + 5) / LedBar2_Num;
  steps = constrain(steps, 0, LedBar2_Num - 1);
  setBarBattery(steps);
}

