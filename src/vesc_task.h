#ifndef VESC_TASK_H
#define VESC_TASK_H

#include <Arduino.h>
#include <VescUart.h>
#include <datatypes.h> // Ensure mc_fault_code is available

// Define a public struct that matches VescUart::dataPackage
// VescUart::dataPackage is private in the library, so we need our own definition
struct VescData {
    float avgMotorCurrent;
    float avgInputCurrent;
    float dutyCycleNow;
    float rpm;
    float inpVoltage;
    float ampHours;
    float ampHoursCharged;
    float wattHours;
    float wattHoursCharged;
    long tachometer;
    long tachometerAbs;
    float tempMosfet;
    float tempMotor;
    float pidPos;
    uint8_t id;
    mc_fault_code error; 
};

// Start the VESC communication task
void startVescTask();

// Thread-safe access to VESC data
VescData getVescData();

// Set target RPM (thread-safe)
void setVescTargetRpm(float rpm);

#endif
