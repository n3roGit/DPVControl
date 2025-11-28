/*
 * Copyright (C) 2024 Christoph Bubeck <christoph.bubeck@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// DPV Control - Main firmware for ESP32-based dive propulsion vehicle
// Test release notes generation
#include <Arduino.h>

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
#include "vesc_task.h" // Include VESC task
#include <LittleFS.h> // For settings storage

// Global variables
int leakSensorFront = 0;        // Current front leak sensor state (0=OK, 1=LEAK)
int leakSensorBack = 0;         // Current back leak sensor state (0=OK, 1=LEAK)
int leakAlarmPersistent = 0;    // Persistent leak alarm flag (survives reboots, 0=OK, 1=ALARM)
int leakAlarmFrontPersistent = 0; // Persistent front sensor alarm (0=OK, 1=FRONT_ALARM)
int leakAlarmBackPersistent = 0;  // Persistent back sensor alarm (0=OK, 1=BACK_ALARM)
int loopCount = 0;              // Counter for main loop iterations
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

  // Keep essential startup messages unconditional for debugging
  log("Booting started...!");

  // Initialize LittleFS first for settings
  if (!LittleFS.begin(true)) {
    log("LittleFS initialization failed!");
  } else {
    log("LittleFS initialized for settings");
  }
  
  // Initialize settings system BEFORE hardware that depends on settings
  initializeSettings();
  loadSettings();

  // Initialize button control system
  buttonSetup();

  // Setup DHT22 sensor with error handling
  log("Initializing DHT22 sensor...");
  dhtSensor.setup(PIN_DHT, DHTesp::DHT22);
  delay(2000); // Give DHT sensor time to stabilize
  
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  if (isnan(data.temperature) || isnan(data.humidity)) {
    log("DHT22 sensor not ready, using default values");
    log("Temp: 20.0°C (default)");
    log("Humidity: 50.0% (default)");
  } else {
    log("Temp: " + String(data.temperature, 2) + "°C");
    log("Humidity: " + String(data.humidity, 1) + "%");
  }
  log("---");

  // Initialize hardware subsystems (now with settings available)
  // Must be initialized BEFORE startVescTask because vescTask might use them (e.g. battery level updates)
  motorSetup();     // Motor control logic
  ledLampSetup();   // Main LED lamp PWM control
  ledBarSetup();    // LED status bar (now with proper settings)
  batterySetup();   // Battery monitoring

  // Start VESC communication task on Core 0
  // This handles all UART communication with the motor controller
  startVescTask();
  
  // Initialize datalogger (will re-initialize LittleFS if needed)
  datalogSetup();

  // Load persistent leak alarm state
  loadPersistentLeakAlarm();
  
  // Debug current alarm states at startup
  debugLeakAlarmStates();

  // Initialize webserver on Core 0
  setupWebserver();

  // Booting finished - keep unconditional for debugging
  log("Booting finished!");
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
  // GetVESCValues();    // REMOVED: Now handled by background task
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
