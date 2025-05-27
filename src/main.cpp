#include "main.h"

#include "constants.h"
#include "Blinker.h" 
#include "BlinkSequence.h" 
#include "log.h"
#include "beep.h"
#include "other.h"
#include "button.h"
#include "datalog.h"
#include "webserver.h"  // Include webserver header
#include "data_upload.h" // Include data upload header
#include "settings.h" // Include DPV settings system
#include <LittleFS.h> // For settings storage

// Global variables
int leakSensorState = 0;  // Current state of leak sensors
int loopCount = 0;        // Counter for main loop iterations
int NormalLogOutputIntervall = 1000*10;  // Normal logging interval (10 seconds)

DHTesp dhtSensor;  // Temperature and humidity sensor

/**
 * System initialization
 * Sets up all hardware components and subsystems
 */
void setup() {
  // Configure GPIO pins
  pinMode(PIN_LEFT_BUTTON, INPUT);      // Left control button
  pinMode(PIN_RIGHT_BUTTON, INPUT);     // Right control button
  pinMode(PIN_LEAK_FRONT, INPUT_PULLUP); // Front leak sensor with pullup
  pinMode(PIN_LEAK_BACK, INPUT_PULLUP);  // Back leak sensor with pullup
  pinMode(PIN_LAMP, OUTPUT);            // Main LED lamp
  pinMode(PIN_BEEP, OUTPUT);            // Buzzer/beeper

  // Initialize serial communication
  Serial.begin(115200);

  Serial.println("Booting started...!");

  // Initialize button control system
  buttonSetup();

  // Setup DHT22 sensor with error handling
  Serial.println("Initializing DHT22 sensor...");
  dhtSensor.setup(PIN_DHT, DHTesp::DHT22);
  delay(2000); // Give DHT sensor time to stabilize
  
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  if (isnan(data.temperature) || isnan(data.humidity)) {
    Serial.println("DHT22 sensor not ready, using default values");
    Serial.println("Temp: 20.0°C (default)");
    Serial.println("Humidity: 50.0% (default)");
  } else {
    Serial.println("Temp: " + String(data.temperature, 2) + "°C");
    Serial.println("Humidity: " + String(data.humidity, 1) + "%");
  }
  Serial.println("---");

  // Initialize hardware subsystems
  motorSetup();     // Motor control and VESC communication
  ledLampSetup();   // Main LED lamp PWM control
  ledBarSetup();    // LED status bar
  batterySetup();   // Battery monitoring
  
  // Initialize LittleFS first for settings
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS initialization failed!");
  } else {
    Serial.println("LittleFS initialized for settings");
  }
  
  // Initialize settings system
  initializeSettings();
  
  // Initialize datalogger (will re-initialize LittleFS if needed)
  datalogSetup();

  // Initialize webserver on Core 0
  setupWebserver();

  // Booting finished
  Serial.println("Booting finished!");
  beep("1");
}

/**
 * Main system loop
 * Handles all real-time operations and system monitoring
 */
void loop() {
  long loopStart = millis();
  loopCount++;

  // Core system operations
  buttonLoop();           // Process button inputs and actions
  motorLoop();           // Update motor control and speed
  checkForLeak();        // Monitor leak sensors
  GetVESCValues();       // Read motor controller data
  logVehicleState();     // Log current system state
  FromTimeToTimeExecution(); // Periodic maintenance tasks
  beepLoop();            // Handle beeper sequences
  ledLampLoop();         // Update LED lamp states
  datalogLoop();         // Data logging operations

  // Performance monitoring
  long loopEnd = millis();
  long diff = loopEnd-loopStart;
  if (diff > 30){
    log("Loop took", diff, true);  // Warn if loop takes too long
  }
  
  delay(1);  // Small delay to prevent watchdog issues
}

