#include "mock_hardware.h"
#include <string>
#include <mutex>
#include <cmath>

// Mock hardware variables
int LED_State = 0;
int currentMotorStep = 0;
bool remoteControlActive = false;
unsigned long lastActionTime = 0;
std::array<bool, 20> mockLEDBarStates = {false};
std::array<uint32_t, 20> mockLEDBarColors = {0};
int mockLEDBarBrightness = 15;
int mockLEDBarBrightnessSecond = 3;
bool mockLEDUpdateInProgress = false;
int mockLastDisplayedSpeed = -1;
int mockLastDisplayedMotorState = -1;
int mockLastDisplayedBattery = -1;
bool mockWaterSensorFront = false;
bool mockWaterSensorBack = false;
float mockTemperature = 0.0f;
float mockHumidity = 0.0f;
bool mockDHTValid = true;
bool mockDHTError = false;
bool mockDHTTimeout = false;

std::mutex mockMutex;

// Helper functions
uint32_t mock_color(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

void mock_getStripBoundaries(int stripNumber, int& startIndex, int& endIndex) {
    if (stripNumber == 1) {
        startIndex = 0;
        endIndex = 10;
    } else if (stripNumber == 2) {
        startIndex = 10;
        endIndex = 20;
    } else {
        startIndex = 0;
        endIndex = 0;
    }
}

bool mock_safeSetPixelColor(int index, uint32_t color) {
    if (index >= 0 && index < 20) {
        mockLEDBarColors[index] = color;
        mockLEDBarStates[index] = true;
        return true;
    }
    return false;
}

int mock_calculateBrightnessCorrectedValue(int red, int green, int blue, int targetBrightness) {
    int activeChannels = 0;
    if (red > 25) activeChannels++;
    if (green > 25) activeChannels++;
    if (blue > 25) activeChannels++;
    
    if (activeChannels == 0) return targetBrightness;
    
    float correctionFactor = 1.0f / std::sqrt(activeChannels);
    int correctedBrightness = (int)(targetBrightness * correctionFactor);
    
    return std::max(1, std::min(100, correctedBrightness));
}

// Mock hardware functions
void mock_ledBarSetup() {
    std::lock_guard<std::mutex> lock(mockMutex);
    
    // Clear all LEDs
    for (int i = 0; i < 20; i++) {
        mockLEDBarStates[i] = false;
        mockLEDBarColors[i] = 0;
    }
    
    // Run startup animation
    mock_knightRiderStartup();
    
    // Set initial states
    mock_setBarStandby();
    mock_setBarBattery(5); // 50% battery
}

void mock_setBarSpeed(int speed) {
    std::lock_guard<std::mutex> lock(mockMutex);
    if (mockLEDUpdateInProgress) return;
    mockLEDUpdateInProgress = true;
    
    if (speed == mockLastDisplayedSpeed) {
        mockLEDUpdateInProgress = false;
        return;
    }
    
    int startIndex, endIndex;
    mock_getStripBoundaries(1, startIndex, endIndex);
    
    speed = std::max(0, std::min(10, speed));
    
    // Set speed LEDs
    for (int i = startIndex; i < startIndex + speed; i++) {
        mock_safeSetPixelColor(i, mock_color(0, 255, 0));
    }
    
    // Clear remaining LEDs
    for (int i = startIndex + speed; i < endIndex; i++) {
        mockLEDBarStates[i] = false;
        mockLEDBarColors[i] = 0;
    }
    
    mockLastDisplayedSpeed = speed;
    mock_forceRefreshLedBar();
    mockLEDUpdateInProgress = false;
}

void mock_setLEDState(int state) {
    std::lock_guard<std::mutex> lock(mockMutex);
    LED_State = state;
}

void mock_setBarLED(int level) {
    std::lock_guard<std::mutex> lock(mockMutex);
    if (mockLEDUpdateInProgress) return;
    mockLEDUpdateInProgress = true;
    
    int startIndex, endIndex;
    mock_getStripBoundaries(2, startIndex, endIndex);
    
    level = std::max(0, std::min(10, level));
    
    // Set level LEDs
    for (int i = startIndex; i < startIndex + level; i++) {
        mock_safeSetPixelColor(i, mock_color(255, 0, 0));
    }
    
    // Clear remaining LEDs
    for (int i = startIndex + level; i < endIndex; i++) {
        mockLEDBarStates[i] = false;
        mockLEDBarColors[i] = 0;
    }
    
    mock_forceRefreshLedBar();
    mockLEDUpdateInProgress = false;
}

void mock_setBarStandby() {
    std::lock_guard<std::mutex> lock(mockMutex);
    if (mockLEDUpdateInProgress) return;
    mockLEDUpdateInProgress = true;
    
    // Set alternating pattern
    for (int i = 0; i < 20; i++) {
        if (i % 2 == 0) {
            mock_safeSetPixelColor(i, mock_color(0, 0, 255));
        } else {
            mockLEDBarStates[i] = false;
            mockLEDBarColors[i] = 0;
        }
    }
    
    mock_forceRefreshLedBar();
    mockLEDUpdateInProgress = false;
}

void mock_setBarSpeedCruise(int num) {
    std::lock_guard<std::mutex> lock(mockMutex);
    if (mockLEDUpdateInProgress) return;
    mockLEDUpdateInProgress = true;
    
    int startIndex, endIndex;
    mock_getStripBoundaries(1, startIndex, endIndex);
    
    num = std::max(0, std::min(10, num));
    
    // Set cruise speed LEDs
    for (int i = startIndex; i < startIndex + num; i++) {
        mock_safeSetPixelColor(i, mock_color(0, 255, 255));
    }
    
    // Clear remaining LEDs
    for (int i = startIndex + num; i < endIndex; i++) {
        mockLEDBarStates[i] = false;
        mockLEDBarColors[i] = 0;
    }
    
    mockLastDisplayedSpeed = num;
    mock_forceRefreshLedBar();
    mockLEDUpdateInProgress = false;
}

void mock_setBarBattery(int num) {
    std::lock_guard<std::mutex> lock(mockMutex);
    if (mockLEDUpdateInProgress) return;
    mockLEDUpdateInProgress = true;
    
    if (num == mockLastDisplayedBattery) {
        mockLEDUpdateInProgress = false;
        return;
    }
    
    int startIndex, endIndex;
    mock_getStripBoundaries(2, startIndex, endIndex);
    
    num = std::max(0, std::min(10, num));
    
    // Set battery LEDs
    for (int i = startIndex; i < startIndex + num; i++) {
        mock_safeSetPixelColor(i, mock_color(0, 255, 0));
    }
    
    // Clear remaining LEDs
    for (int i = startIndex + num; i < endIndex; i++) {
        mockLEDBarStates[i] = false;
        mockLEDBarColors[i] = 0;
    }
    
    mockLastDisplayedBattery = num;
    mock_forceRefreshLedBar();
    mockLEDUpdateInProgress = false;
}

void mock_setBarLeak() {
    std::lock_guard<std::mutex> lock(mockMutex);
    if (mockLEDUpdateInProgress) return;
    mockLEDUpdateInProgress = true;
    
    // Set leak warning pattern
    for (int i = 0; i < 20; i++) {
        if (i % 3 == 0) {
            mock_safeSetPixelColor(i, mock_color(255, 0, 0));
        } else {
            mockLEDBarStates[i] = false;
            mockLEDBarColors[i] = 0;
        }
    }
    
    mock_forceRefreshLedBar();
    mockLEDUpdateInProgress = false;
}

void mock_setBarPowerBank(bool status) {
    std::lock_guard<std::mutex> lock(mockMutex);
    if (mockLEDUpdateInProgress) return;
    mockLEDUpdateInProgress = true;
    
    // Update power bank indicator (last LED)
    if (status) {
        mock_safeSetPixelColor(19, mock_color(0, 255, 0));
    } else {
        mockLEDBarStates[19] = false;
        mockLEDBarColors[19] = 0;
    }
    
    mock_forceRefreshLedBar();
    mockLEDUpdateInProgress = false;
}

void mock_setBarFlasher(bool status) {
    std::lock_guard<std::mutex> lock(mockMutex);
    if (mockLEDUpdateInProgress) return;
    mockLEDUpdateInProgress = true;
    
    // Set flasher pattern
    for (int i = 0; i < 20; i++) {
        if (status && i % 2 == 0) {
            mock_safeSetPixelColor(i, mock_color(255, 255, 0));
        } else {
            mockLEDBarStates[i] = false;
            mockLEDBarColors[i] = 0;
        }
    }
    
    mock_forceRefreshLedBar();
    mockLEDUpdateInProgress = false;
}

void mock_forceRefreshLedBar() {
    lastActionTime = millis();
}

void mock_knightRiderStartup() {
    std::lock_guard<std::mutex> lock(mockMutex);
    
    // Clear all LEDs
    for (int i = 0; i < 20; i++) {
        mockLEDBarStates[i] = false;
        mockLEDBarColors[i] = 0;
    }
    
    // Simulate Knight Rider animation
    for (int i = 0; i < 20; i++) {
        mock_safeSetPixelColor(i, mock_color(255, 0, 0));
        mock_forceRefreshLedBar();
        mockLEDBarStates[i] = false;
        mockLEDBarColors[i] = 0;
    }
    
    for (int i = 19; i >= 0; i--) {
        mock_safeSetPixelColor(i, mock_color(255, 0, 0));
        mock_forceRefreshLedBar();
        mockLEDBarStates[i] = false;
        mockLEDBarColors[i] = 0;
    }
}

void mock_setWaterSensorFront(bool state) {
    std::lock_guard<std::mutex> lock(mockMutex);
    mockWaterSensorFront = state;
}

void mock_setWaterSensorBack(bool state) {
    std::lock_guard<std::mutex> lock(mockMutex);
    mockWaterSensorBack = state;
}

bool mock_getWaterSensorFront() {
    std::lock_guard<std::mutex> lock(mockMutex);
    return mockWaterSensorFront;
}

bool mock_getWaterSensorBack() {
    std::lock_guard<std::mutex> lock(mockMutex);
    return mockWaterSensorBack;
}

void mock_setTemperature(float temp) {
    std::lock_guard<std::mutex> lock(mockMutex);
    if (temp < -40.0f) temp = -40.0f;
    if (temp > 80.0f) temp = 80.0f;
    mockTemperature = temp;
    mockDHTValid = true;
    mockDHTError = false;
    mockDHTTimeout = false;
}

void mock_setHumidity(float hum) {
    std::lock_guard<std::mutex> lock(mockMutex);
    if (hum < 0.0f) hum = 0.0f;
    if (hum > 100.0f) hum = 100.0f;
    mockHumidity = hum;
    mockDHTValid = true;
    mockDHTError = false;
    mockDHTTimeout = false;
}

float mock_getTemperature() {
    std::lock_guard<std::mutex> lock(mockMutex);
    if (!mockDHTValid) return 0.0f;
    return mockTemperature;
}

float mock_getHumidity() {
    std::lock_guard<std::mutex> lock(mockMutex);
    if (!mockDHTValid) return 0.0f;
    return mockHumidity;
}

void mock_setDHTError(bool error) {
    std::lock_guard<std::mutex> lock(mockMutex);
    mockDHTError = error;
    if (error) {
        mockDHTValid = false;
        mockTemperature = 0.0f;
        mockHumidity = 0.0f;
    }
}

void mock_setDHTTimeout(bool timeout) {
    std::lock_guard<std::mutex> lock(mockMutex);
    mockDHTTimeout = timeout;
    if (timeout) {
        mockDHTValid = false;
        mockTemperature = 0.0f;
        mockHumidity = 0.0f;
    }
}

bool mock_getDHTValid() {
    std::lock_guard<std::mutex> lock(mockMutex);
    return mockDHTValid && !mockDHTError && !mockDHTTimeout;
}

int mock_getTotalDataPoints(const std::string& timeRange) {
    return 100; // Mock value
}

std::string* mock_listSessionFiles(int* count) {
    *count = 0;
    return nullptr;
}

std::string mock_getCurrentSessionFile() {
    return "current_session.json";
}

float mock_computeHeatIndex(float temperature, float humidity) {
    // Simplified heat index calculation
    if (temperature < 20.0) return temperature;
    
    float heatIndex = 0.5 * (temperature + 61.0 + ((temperature - 68.0) * 1.2) + (humidity * 0.094));
    if (heatIndex < 80.0) return heatIndex;
    
    heatIndex = -42.379 + 2.04901523 * temperature + 10.14333127 * humidity
                - 0.22475541 * temperature * humidity - 0.00683783 * temperature * temperature
                - 0.05481717 * humidity * humidity + 0.00122874 * temperature * temperature * humidity
                + 0.00085282 * temperature * humidity * humidity - 0.00000199 * temperature * temperature * humidity * humidity;
    
    return heatIndex;
}

float mock_computeDewPoint(float temperature, float humidity) {
    // Simplified dew point calculation
    float a = 17.27;
    float b = 237.7;
    float temp = (a * temperature) / (b + temperature) + log(humidity / 100.0);
    return (b * temp) / (a - temp);
}

float mock_getComfortRatio(ComfortState& comfort, float temperature, float humidity) {
    // Simplified comfort ratio calculation
    float ratio = 100.0;
    
    // Temperature comfort
    if (temperature > 30.0) {
        comfort = Comfort_TooHot;
        ratio -= (temperature - 30.0) * 10.0;
    } else if (temperature < 18.0) {
        comfort = Comfort_TooCold;
        ratio -= (18.0 - temperature) * 10.0;
    } else {
        comfort = Comfort_OK;
    }
    
    // Humidity comfort
    if (humidity > 70.0) {
        ratio -= (humidity - 70.0) * 0.5;
    } else if (humidity < 30.0) {
        ratio -= (30.0 - humidity) * 0.5;
    }
    
    return max(0.0f, min(100.0f, ratio));
}

byte mock_computePerception(float temperature, float humidity) {
    // Simplified perception calculation
    if (temperature < 15.0) return Perception_Cold;
    if (temperature > 30.0) return Perception_Hot;
    
    if (humidity < 30.0) return Perception_Dry;
    if (humidity > 70.0) return Perception_Wet;
    
    if (temperature >= 22.0 && temperature <= 26.0 && humidity >= 40.0 && humidity <= 60.0) {
        return Perception_Comfy;
    }
    
    return Perception_UnComfy;
}

float mock_computeAbsoluteHumidity(float temperature, float humidity) {
    // Simplified absolute humidity calculation (g/m³)
    float vaporPressure = (humidity / 100.0) * 6.112 * exp(17.67 * temperature / (temperature + 243.5));
    return (vaporPressure * 100.0) / (461.5 * (temperature + 273.15));
} 