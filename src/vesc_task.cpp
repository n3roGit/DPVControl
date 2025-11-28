#include "vesc_task.h"
#include "constants.h"
#include "log.h"
#include "battery.h" // For updateBatteryLevel

// Global VESC instance (only accessed by this task)
VescUart UART;

// Thread-safe data storage
SemaphoreHandle_t vescDataMutex = NULL;
VescData protectedVescData;
float targetRpm = 0.0;

// Task handle
TaskHandle_t vescTaskHandle = NULL;

// Helper to copy data from VescUart::dataPackage to VescData
void updateProtectedData() {
    if (xSemaphoreTake(vescDataMutex, 10 / portTICK_PERIOD_MS) == pdTRUE) {
        protectedVescData.avgMotorCurrent = UART.data.avgMotorCurrent;
        protectedVescData.avgInputCurrent = UART.data.avgInputCurrent;
        protectedVescData.dutyCycleNow = UART.data.dutyCycleNow;
        protectedVescData.rpm = UART.data.rpm;
        protectedVescData.inpVoltage = UART.data.inpVoltage;
        protectedVescData.ampHours = UART.data.ampHours;
        protectedVescData.ampHoursCharged = UART.data.ampHoursCharged;
        protectedVescData.wattHours = UART.data.wattHours;
        protectedVescData.wattHoursCharged = UART.data.wattHoursCharged;
        protectedVescData.tachometer = UART.data.tachometer;
        protectedVescData.tachometerAbs = UART.data.tachometerAbs;
        protectedVescData.tempMosfet = UART.data.tempMosfet;
        protectedVescData.tempMotor = UART.data.tempMotor;
        protectedVescData.pidPos = UART.data.pidPos;
        protectedVescData.id = UART.data.id;
        protectedVescData.error = UART.data.error;
        xSemaphoreGive(vescDataMutex);
    }
}

void vescTask(void *pvParameters) {
  log("VESC Task started on Core 0");

  // Initialize Serial1 for VESC
  // Note: We assume Serial1 pins are defined in constants.h as VESCRX and VESCTX
  Serial1.begin(115200, SERIAL_8N1, VESCRX, VESCTX);
  while (!Serial1) { vTaskDelay(10 / portTICK_PERIOD_MS); }
  
  UART.setSerialPort(&Serial1);
  
  // Create mutex
  vescDataMutex = xSemaphoreCreateMutex();

  unsigned long lastReadTime = 0;
  const unsigned long READ_INTERVAL = 100; // Read every 100ms

  while (true) {
    // 1. Send Control Command (High Priority)
    // We send this every cycle to keep VESC alive and responsive
    UART.setRPM(targetRpm);

    // 2. Read Data (Lower Priority, periodic)
    unsigned long now = millis();
    if (now - lastReadTime >= READ_INTERVAL) {
      bool success = UART.getVescValues();
      
      if (success) {
        // Update battery level logic
        updateBatteryLevel(UART.data.inpVoltage);

        // Update protected data structure
        updateProtectedData();
      }
      lastReadTime = now;
    }

    // 3. Wait a bit to allow other tasks on Core 0 to run (Webserver, Datalogger)
    // 10ms delay = ~100Hz control loop (minus execution time)
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void startVescTask() {
  // Create task on Core 0
  xTaskCreatePinnedToCore(
    vescTask,
    "VescTask",
    4096,
    NULL,
    1, // Priority 1
    &vescTaskHandle,
    0 // Core 0
  );
}

VescData getVescData() {
  VescData dataCopy;
  // Initialize with zero/default if mutex not ready
  memset(&dataCopy, 0, sizeof(VescData));

  if (vescDataMutex != NULL) {
    if (xSemaphoreTake(vescDataMutex, 5 / portTICK_PERIOD_MS) == pdTRUE) {
      dataCopy = protectedVescData;
      xSemaphoreGive(vescDataMutex);
    }
  }
  return dataCopy;
}

void setVescTargetRpm(float rpm) {
  targetRpm = rpm;
}
