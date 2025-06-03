#ifndef MOCK_DHTESP_H
#define MOCK_DHTESP_H

#include "Arduino.h"

class DHTesp {
public:
    struct TempAndHumidity {
        float temperature;
        float humidity;
    };

    void setup(int pin, int type) {}
    TempAndHumidity getTempAndHumidity() {
        return {25.0f, 50.0f}; // Return mock values
    }
};

#endif // MOCK_DHTESP_H 