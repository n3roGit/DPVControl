/*
* Manages the battery
*/

struct VoltToSoc{
  float volt;
  int soc;
};

/*
*  CONSTANTS
*/

//Mapping of voltage of one of our battery cells to its approximate soc. 
const VoltToSoc VOLT_TO_SOC[] = {
  {4.18, 100},
  {4.1, 96},
  {3.99, 82},
  {3.85, 68},
  {3.77, 58},
  {3.58, 34},
  {3.42, 20},
  {3.33, 14},
  {3.21, 8},
  {3.00, 2},
  {2.87, 0}
};
const int VOLT_TO_SOC_length = sizeof(VOLT_TO_SOC) / sizeof(VOLT_TO_SOC[0]);

const int CELLS_IN_SERIES = 13;//number of cells in series
const int MEASUREMENTS = 60;
const int MEASUREMENT_INTERVAL = 1000;//Time between measurements in ms
const float EMPTY = -3.0;

/*
* GLOBAL VARIABLES
*/
float voltageHistory[MEASUREMENTS] ;
int voltageHistoryIndex = 0;
unsigned long lastMeasurement = 0; //ms timestamp of last time we made measurement


void batterySetup(){
  //Clear measurement table
  for(int i = 0;i<MEASUREMENTS;i++) voltageHistory[i] = EMPTY;
}


/**
*
* Read the state of the main-battery
*
*/
void outputBatteryInfo(){
  log("battery info", batteryLevel);
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

void updateBatteryLevel(float voltage) {
  recordVoltage(voltage);
  batteryLevel = calculateStateOfCharge(getAvergageVoltage());
  int steps = (batteryLevel + 5) / LedBar2_Num;
  steps = constrain(steps, 0, LedBar2_Num - 1);
  setBarBattery(steps);
}

int calculateStateOfCharge(float voltage){
  float voltagePerCell = voltage/CELLS_IN_SERIES;
  //Move along the table until we find the row where we have a lower voltage; 
  int i = 0;
  while (i < VOLT_TO_SOC_length && voltagePerCell < VOLT_TO_SOC[i].volt) i++;
  
  //We did not move along the table at all. So our voltage is above the maxinum.
  if (i == 0) return 100;

  //We made it to the end without finding out voltage. So we are below the minimum. 
  if (i == VOLT_TO_SOC_length) return 0;

  VoltToSoc higher = VOLT_TO_SOC[i-1];
  VoltToSoc lower = VOLT_TO_SOC[i];
  float voltageRange = higher.volt - lower.volt;
  int percentageRange = higher.soc - lower.soc;
  float voltageDifference = voltagePerCell - lower.volt;
  float interpolationFactor = voltageDifference / voltageRange;
  return lower.soc + interpolationFactor * percentageRange;  
}


void recordVoltage(float voltage){
  if (millis() > lastMeasurement + MEASUREMENT_INTERVAL){
    //log("recordVoltage(mV)", (int)(voltage*1000));
    lastMeasurement = millis();
    voltageHistory[voltageHistoryIndex] = voltage;
    //Move Index
    voltageHistoryIndex = voltageHistoryIndex == MEASUREMENTS ? 0 : voltageHistoryIndex + 1;
  }
}

float getAvergageVoltage(){
  float sum = 0.0;
  int i = 0;
  while (i < MEASUREMENTS && voltageHistory[i] != EMPTY){
    sum += voltageHistory[i];
    i++;
  }
  if (i==0) return 0.0;
  return sum/i;
}


/*
* TESTING
*/

//Checks that the two battery calculation functions work the same. 
void testBattery(){
  for(int i = 0;i<40;i++){
    float voltage = 35.0+i*0.5;
    Serial.print(voltage);Serial.print("V ");
    int soc = calculateStateOfCharge(voltage);
    Serial.print(soc);Serial.println("% SOC");
  }
}
