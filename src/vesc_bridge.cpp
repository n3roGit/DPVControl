#include "vesc_bridge.h"

#include <BluetoothSerial.h>

#include "log.h"
#include "vesc_task.h"

// Bluetooth SPP instance used for the VESC bridge.
static BluetoothSerial* vescBridgeSerial = nullptr;

// FreeRTOS task handle for the bridge loop.
static TaskHandle_t vescBridgeTaskHandle = nullptr;

// Current active state of the bridge.
static bool vescBridgeActive = false;

// Forward declaration of the task function
static void vescBridgeTask(void* pvParameters);

bool vescBridgeIsSupported() {
  // For the current ESP32 build the bridge is always available.
  return true;
}

bool vescBridgeIsActive() {
  return vescBridgeActive;
}

bool vescBridgeEnable() {
  if (vescBridgeActive) {
    // Already active
    return true;
  }

  // Pause the regular VESC control loop and make sure the motor is not driven.
  setVescBridgeMode(true);
  setVescTargetRpm(0.0f);

  // Initialize Bluetooth SPP
  vescBridgeSerial = new BluetoothSerial();
  if (!vescBridgeSerial->begin("DPV-VESC-Bridge")) {
    log("VESC Bridge: BluetoothSerial begin() failed");
    delete vescBridgeSerial;
    vescBridgeSerial = nullptr;
    setVescBridgeMode(false);
    return false;
  }

  // Create the bridge task on Core 0 (together with webserver/VESC task)
  BaseType_t result = xTaskCreatePinnedToCore(
      vescBridgeTask,
      "VescBridgeTask",
      4096,
      nullptr,
      1,
      &vescBridgeTaskHandle,
      0);

  if (result != pdPASS) {
    log("VESC Bridge: Failed to create bridge task");
    vescBridgeSerial->end();
    delete vescBridgeSerial;
    vescBridgeSerial = nullptr;
    vescBridgeTaskHandle = nullptr;
    setVescBridgeMode(false);
    return false;
  }

  vescBridgeActive = true;
  log("VESC Bridge enabled");
  return true;
}

void vescBridgeDisable() {
  if (!vescBridgeActive) {
    return;
  }

  log("VESC Bridge disabling");

  // Stop the bridge task first
  if (vescBridgeTaskHandle != nullptr) {
    vTaskDelete(vescBridgeTaskHandle);
    vescBridgeTaskHandle = nullptr;
  }

  // Shut down Bluetooth SPP
  if (vescBridgeSerial != nullptr) {
    vescBridgeSerial->end();
    delete vescBridgeSerial;
    vescBridgeSerial = nullptr;
  }

  // Resume normal VESC control loop
  setVescBridgeMode(false);

  vescBridgeActive = false;
  log("VESC Bridge disabled");
}

static void vescBridgeTask(void* pvParameters) {
  log("VESC Bridge task started on Core 0");

  // Simple transparent bridge between Bluetooth SPP and VESC UART (Serial1)
  const size_t BUFFER_SIZE = 256;
  uint8_t buffer[BUFFER_SIZE];

  while (true) {
    // Forward data from Bluetooth to VESC UART
    if (vescBridgeSerial != nullptr) {
      int availableBt = vescBridgeSerial->available();
      while (availableBt > 0) {
        int toRead = availableBt > static_cast<int>(BUFFER_SIZE)
                         ? static_cast<int>(BUFFER_SIZE)
                         : availableBt;
        int readBytes = vescBridgeSerial->readBytes(
            reinterpret_cast<char*>(buffer), toRead);
        if (readBytes > 0) {
          Serial1.write(buffer, readBytes);
        }
        availableBt = vescBridgeSerial->available();
      }
    }

    // Forward data from VESC UART to Bluetooth
    int availableUart = Serial1.available();
    while (availableUart > 0 && vescBridgeSerial != nullptr) {
      int toRead = availableUart > static_cast<int>(BUFFER_SIZE)
                       ? static_cast<int>(BUFFER_SIZE)
                       : availableUart;
      int readBytes = Serial1.readBytes(
          reinterpret_cast<char*>(buffer), toRead);
      if (readBytes > 0) {
        vescBridgeSerial->write(buffer, readBytes);
      }
      availableUart = Serial1.available();
    }

    // Yield to other tasks
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

