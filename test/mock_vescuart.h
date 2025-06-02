#ifndef MOCK_VESCUART_H
#define MOCK_VESCUART_H

#include "mock_arduino.h"

class VescUart {
public:
    VescUart() {}
    
    bool getVescValues() { return true; }
    bool setNunchuckValues() { return true; }
    bool setCurrent(float current) { return true; }
    bool setBrakeCurrent(float brakeCurrent) { return true; }
    bool setRPM(float rpm) { return true; }
    bool setDuty(float duty) { return true; }
    
    float data_tachometerAbs = 0;
    float data_tachometer = 0;
    float data_inputVoltage = 0;
    float data_temp = 0;
    float data_currentIn = 0;
    float data_currentMotor = 0;
    float data_id = 0;
    float data_iq = 0;
    float data_dutyNow = 0;
    float data_rpm = 0;
    float data_ampHours = 0;
    float data_ampHoursCharged = 0;
    float data_wattHours = 0;
    float data_wattHoursCharged = 0;
    float data_tachometerAbs = 0;
    float data_tachometer = 0;
    float data_temp = 0;
    float data_inputVoltage = 0;
    float data_currentIn = 0;
    float data_currentMotor = 0;
    float data_id = 0;
    float data_iq = 0;
    float data_dutyNow = 0;
    float data_rpm = 0;
    float data_ampHours = 0;
    float data_ampHoursCharged = 0;
    float data_wattHours = 0;
    float data_wattHoursCharged = 0;
};

#endif // MOCK_VESCUART_H 