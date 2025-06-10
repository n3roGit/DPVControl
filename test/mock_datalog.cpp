#include "mock_datalog.h"
#include "mock_arduino.h"

// Mock implementations of datalog functions

unsigned long getTotalUptime() {
    return bootTimeSeconds * 1000 + millis();
}

LogdataRow createOptimizedDatapoint(unsigned long currentTime) {
    LogdataRow dp;
    dp.timestamp = currentTime;
    dp.totalUptime = getTotalUptime();
    
    // Set mock values for testing
    dp.tempMotor = 25.0;
    dp.tempMosfet = 30.0;
    dp.batteryVoltage = 52.0;
    dp.current = 0.0;
    dp.avgMotorCurrent = 0.0;
    dp.erpm = 0.0;
    dp.dutyCycle = 0.0;
    dp.temperature = 24.0;
    dp.humidity = 50.0;
    dp.batteryLevel = 85;
    dp.leakSensorState = 0;
    dp.ledBrightness = 0;
    dp.leftButton = 0;
    dp.rightButton = 0;
    dp.beeperEnabled = 1;
    dp.beeperActive = 0;
    
    return dp;
}

bool isValidRuntime(unsigned long runtime) {
    // Valid runtime: 0 to ~31 years (1 billion seconds)
    return runtime <= 1000000000UL;
} 