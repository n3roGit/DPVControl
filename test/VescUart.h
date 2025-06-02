#pragma once

class VescUart {
public:
    VescUart() {}
    void setCurrent(float) {}
    void setRPM(int) {}
    void setDuty(float) {}
    float getVoltage() { return 42.0f; }
    float getCurrent() { return 0.0f; }
}; 