#include "mock_hardware.h"
#include <chrono>
#include <thread>

// Initialize mock hardware states
int mockLEDState = 0;
int mockMotorStep = 0;
bool mockBeeperEnabled = false;
bool mockWaterSensorFront = false;
bool mockWaterSensorBack = false;
bool mockLeftButton = false;
bool mockRightButton = false;

// Mock LED bar states
std::array<bool, 20> mockLEDBarStates = {false};
int mockLEDBarBrightness = 100;
int mockLEDBarSection1Brightness = 100;
int mockLEDBarSection2Brightness = 100;

// Mock front lamp PWM state
int mockFrontLampPWM = 0;
int mockFrontLampFrequency = 5000;  // 5kHz default
int mockFrontLampResolution = 8;    // 8-bit resolution default

// Mock click code states
std::vector<int> mockClickCode;
bool mockClickCodeActive = false;
int mockClickCodeTimeout = 5000; // 5 seconds default timeout

// Mock network states
bool mockNetworkConnected = true;
int mockNetworkLatency = 0;
int mockNetworkErrorRate = 0;
std::queue<std::string> mockNetworkErrors;
std::mutex networkMutex;

// Mock memory states
size_t mockFreeHeap = 100000;
size_t mockTotalHeap = 200000;
size_t mockMaxAllocation = 50000;
bool mockMemoryError = false;
std::mutex memoryMutex;

// Mock security states
bool mockAccessGranted = true;
int mockRateLimit = 100;
int mockRequestCount = 0;
std::vector<std::string> mockInvalidInputs;
std::mutex securityMutex;

// Mock performance states
int mockProcessingTime = 0;
int mockConcurrentRequests = 0;
size_t mockDataSize = 0;
std::mutex performanceMutex;

// Mock hardware function implementations
void mockSetLEDState(int state) {
    mockLEDState = state;
}

int mockGetLEDState() {
    return mockLEDState;
}

void mockSetMotorStep(int step) {
    mockMotorStep = step;
}

int mockGetMotorStep() {
    return mockMotorStep;
}

void mockSetBeeperEnabled(bool enabled) {
    mockBeeperEnabled = enabled;
}

bool mockGetBeeperEnabled() {
    return mockBeeperEnabled;
}

void mockSetWaterSensorFront(bool state) {
    mockWaterSensorFront = state;
}

bool mockGetWaterSensorFront() {
    return mockWaterSensorFront;
}

void mockSetWaterSensorBack(bool state) {
    mockWaterSensorBack = state;
}

bool mockGetWaterSensorBack() {
    return mockWaterSensorBack;
}

void mockSetLeftButton(bool state) {
    mockLeftButton = state;
}

bool mockGetLeftButton() {
    return mockLeftButton;
}

void mockSetRightButton(bool state) {
    mockRightButton = state;
}

bool mockGetRightButton() {
    return mockRightButton;
}

// Mock LED bar functions
void mockSetLEDBarState(int index, bool state) {
    if (index >= 0 && index < 20) {
        mockLEDBarStates[index] = state;
    }
}

bool mockGetLEDBarState(int index) {
    if (index >= 0 && index < 20) {
        return mockLEDBarStates[index];
    }
    return false;
}

void mockSetLEDBarBrightness(int brightness) {
    if (brightness >= 0 && brightness <= 100) {
        mockLEDBarBrightness = brightness;
        mockLEDBarSection1Brightness = brightness;
        mockLEDBarSection2Brightness = brightness;
    }
}

int mockGetLEDBarBrightness() {
    return mockLEDBarBrightness;
}

void mockSetLEDBarSectionBrightness(int section, int brightness) {
    if (brightness >= 0 && brightness <= 100) {
        if (section == 1) {
            mockLEDBarSection1Brightness = brightness;
        } else if (section == 2) {
            mockLEDBarSection2Brightness = brightness;
        }
    }
}

int mockGetLEDBarSectionBrightness(int section) {
    if (section == 1) {
        return mockLEDBarSection1Brightness;
    } else if (section == 2) {
        return mockLEDBarSection2Brightness;
    }
    return 0;
}

void mockSetLEDBarPattern(const std::array<bool, 20>& pattern) {
    mockLEDBarStates = pattern;
}

std::array<bool, 20> mockGetLEDBarPattern() {
    return mockLEDBarStates;
}

void mockSetLEDBarSectionPattern(int section, const std::array<bool, 10>& pattern) {
    if (section == 1) {
        for (int i = 0; i < 10; i++) {
            mockLEDBarStates[i] = pattern[i];
        }
    } else if (section == 2) {
        for (int i = 0; i < 10; i++) {
            mockLEDBarStates[i + 10] = pattern[i];
        }
    }
}

std::array<bool, 10> mockGetLEDBarSectionPattern(int section) {
    std::array<bool, 10> pattern = {false};
    if (section == 1) {
        for (int i = 0; i < 10; i++) {
            pattern[i] = mockLEDBarStates[i];
        }
    } else if (section == 2) {
        for (int i = 0; i < 10; i++) {
            pattern[i] = mockLEDBarStates[i + 10];
        }
    }
    return pattern;
}

void mockClearLEDBar() {
    mockLEDBarStates.fill(false);
}

void mockClearLEDBarSection(int section) {
    if (section == 1) {
        for (int i = 0; i < 10; i++) {
            mockLEDBarStates[i] = false;
        }
    } else if (section == 2) {
        for (int i = 10; i < 20; i++) {
            mockLEDBarStates[i] = false;
        }
    }
}

void mockSetAllLEDBar(bool state) {
    mockLEDBarStates.fill(state);
}

void mockSetAllLEDBarSection(int section, bool state) {
    if (section == 1) {
        for (int i = 0; i < 10; i++) {
            mockLEDBarStates[i] = state;
        }
    } else if (section == 2) {
        for (int i = 10; i < 20; i++) {
            mockLEDBarStates[i] = state;
        }
    }
}

// Mock front lamp PWM functions
void mockSetFrontLampPWM(int value) {
    if (value >= 0 && value < (1 << mockFrontLampResolution)) {
        mockFrontLampPWM = value;
    }
}

int mockGetFrontLampPWM() {
    return mockFrontLampPWM;
}

void mockSetFrontLampFrequency(int frequency) {
    if (frequency > 0) {
        mockFrontLampFrequency = frequency;
    }
}

int mockGetFrontLampFrequency() {
    return mockFrontLampFrequency;
}

void mockSetFrontLampResolution(int resolution) {
    if (resolution > 0 && resolution <= 16) {
        mockFrontLampResolution = resolution;
        // Adjust PWM value to new resolution
        if (mockFrontLampPWM >= (1 << resolution)) {
            mockFrontLampPWM = (1 << resolution) - 1;
        }
    }
}

int mockGetFrontLampResolution() {
    return mockFrontLampResolution;
}

void mockSetFrontLampDutyCycle(float dutyCycle) {
    if (dutyCycle >= 0.0f && dutyCycle <= 100.0f) {
        int maxValue = (1 << mockFrontLampResolution) - 1;
        mockFrontLampPWM = static_cast<int>((dutyCycle / 100.0f) * maxValue);
    }
}

float mockGetFrontLampDutyCycle() {
    int maxValue = (1 << mockFrontLampResolution) - 1;
    return (static_cast<float>(mockFrontLampPWM) / maxValue) * 100.0f;
}

// Mock click code functions
void mockSetClickCode(const std::vector<int>& code) {
    mockClickCode = code;
}

std::vector<int> mockGetClickCode() {
    return mockClickCode;
}

void mockSetClickCodeActive(bool active) {
    mockClickCodeActive = active;
}

bool mockGetClickCodeActive() {
    return mockClickCodeActive;
}

void mockSetClickCodeTimeout(int timeout) {
    if (timeout > 0) {
        mockClickCodeTimeout = timeout;
    }
}

int mockGetClickCodeTimeout() {
    return mockClickCodeTimeout;
}

void mockAddClick(int duration) {
    if (mockClickCodeActive) {
        mockClickCode.push_back(duration);
    }
}

void mockClearClickCode() {
    mockClickCode.clear();
}

bool mockVerifyClickCode(const std::vector<int>& code) {
    if (code.size() != mockClickCode.size()) {
        return false;
    }
    
    for (size_t i = 0; i < code.size(); i++) {
        if (code[i] != mockClickCode[i]) {
            return false;
        }
    }
    
    return true;
}

// Mock network functions
void mockSetNetworkConnected(bool connected) {
    std::lock_guard<std::mutex> lock(networkMutex);
    mockNetworkConnected = connected;
}

bool mockGetNetworkConnected() {
    std::lock_guard<std::mutex> lock(networkMutex);
    return mockNetworkConnected;
}

void mockSetNetworkLatency(int latency) {
    std::lock_guard<std::mutex> lock(networkMutex);
    mockNetworkLatency = latency;
}

int mockGetNetworkLatency() {
    std::lock_guard<std::mutex> lock(networkMutex);
    return mockNetworkLatency;
}

void mockSetNetworkErrorRate(int rate) {
    std::lock_guard<std::mutex> lock(networkMutex);
    mockNetworkErrorRate = rate;
}

int mockGetNetworkErrorRate() {
    std::lock_guard<std::mutex> lock(networkMutex);
    return mockNetworkErrorRate;
}

void mockAddNetworkError(const std::string& error) {
    std::lock_guard<std::mutex> lock(networkMutex);
    mockNetworkErrors.push(error);
}

std::string mockGetNextNetworkError() {
    std::lock_guard<std::mutex> lock(networkMutex);
    if (mockNetworkErrors.empty()) {
        return "";
    }
    std::string error = mockNetworkErrors.front();
    mockNetworkErrors.pop();
    return error;
}

bool mockHasNetworkErrors() {
    std::lock_guard<std::mutex> lock(networkMutex);
    return !mockNetworkErrors.empty();
}

// Mock memory functions
void mockSetFreeHeap(size_t size) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    mockFreeHeap = size;
}

size_t mockGetFreeHeap() {
    std::lock_guard<std::mutex> lock(memoryMutex);
    return mockFreeHeap;
}

void mockSetTotalHeap(size_t size) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    mockTotalHeap = size;
}

size_t mockGetTotalHeap() {
    std::lock_guard<std::mutex> lock(memoryMutex);
    return mockTotalHeap;
}

void mockSetMaxAllocation(size_t size) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    mockMaxAllocation = size;
}

size_t mockGetMaxAllocation() {
    std::lock_guard<std::mutex> lock(memoryMutex);
    return mockMaxAllocation;
}

void mockSetMemoryError(bool error) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    mockMemoryError = error;
}

bool mockGetMemoryError() {
    std::lock_guard<std::mutex> lock(memoryMutex);
    return mockMemoryError;
}

bool mockAllocateMemory(size_t size) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    if (mockMemoryError || size > mockFreeHeap || size > mockMaxAllocation) {
        return false;
    }
    mockFreeHeap -= size;
    return true;
}

void mockFreeMemory(size_t size) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    mockFreeHeap += size;
}

// Mock security functions
void mockSetAccessGranted(bool granted) {
    std::lock_guard<std::mutex> lock(securityMutex);
    mockAccessGranted = granted;
}

bool mockGetAccessGranted() {
    std::lock_guard<std::mutex> lock(securityMutex);
    return mockAccessGranted;
}

void mockSetRateLimit(int limit) {
    std::lock_guard<std::mutex> lock(securityMutex);
    mockRateLimit = limit;
}

int mockGetRateLimit() {
    std::lock_guard<std::mutex> lock(securityMutex);
    return mockRateLimit;
}

void mockResetRequestCount() {
    std::lock_guard<std::mutex> lock(securityMutex);
    mockRequestCount = 0;
}

int mockGetRequestCount() {
    std::lock_guard<std::mutex> lock(securityMutex);
    return mockRequestCount;
}

void mockAddInvalidInput(const std::string& input) {
    std::lock_guard<std::mutex> lock(securityMutex);
    mockInvalidInputs.push_back(input);
}

bool mockIsInputValid(const std::string& input) {
    std::lock_guard<std::mutex> lock(securityMutex);
    return std::find(mockInvalidInputs.begin(), mockInvalidInputs.end(), input) == mockInvalidInputs.end();
}

void mockClearInvalidInputs() {
    std::lock_guard<std::mutex> lock(securityMutex);
    mockInvalidInputs.clear();
}

// Mock performance functions
void mockSetProcessingTime(int time) {
    std::lock_guard<std::mutex> lock(performanceMutex);
    mockProcessingTime = time;
}

int mockGetProcessingTime() {
    std::lock_guard<std::mutex> lock(performanceMutex);
    return mockProcessingTime;
}

void mockSetConcurrentRequests(int count) {
    std::lock_guard<std::mutex> lock(performanceMutex);
    mockConcurrentRequests = count;
}

int mockGetConcurrentRequests() {
    std::lock_guard<std::mutex> lock(performanceMutex);
    return mockConcurrentRequests;
}

void mockSetDataSize(size_t size) {
    std::lock_guard<std::mutex> lock(performanceMutex);
    mockDataSize = size;
}

size_t mockGetDataSize() {
    std::lock_guard<std::mutex> lock(performanceMutex);
    return mockDataSize;
}

void mockSimulateLoad(int duration) {
    std::this_thread::sleep_for(std::chrono::milliseconds(duration));
}

void mockResetPerformance() {
    std::lock_guard<std::mutex> lock(performanceMutex);
    mockProcessingTime = 0;
    mockConcurrentRequests = 0;
    mockDataSize = 0;
}

// Mock integration functions
void mockResetAllStates() {
    mockSetNetworkConnected(true);
    mockSetNetworkLatency(0);
    mockSetNetworkErrorRate(0);
    mockSetFreeHeap(100000);
    mockSetTotalHeap(200000);
    mockSetMemoryError(false);
    mockSetAccessGranted(true);
    mockSetRateLimit(100);
    mockResetRequestCount();
    mockClearInvalidInputs();
    mockResetPerformance();
}

void mockSimulateStateChange(const std::string& component, const std::string& state) {
    if (component == "network") {
        mockSetNetworkConnected(state == "connected");
    } else if (component == "memory") {
        mockSetMemoryError(state == "error");
    } else if (component == "security") {
        mockSetAccessGranted(state == "granted");
    }
}

std::string mockGetComponentState(const std::string& component) {
    if (component == "network") {
        return mockGetNetworkConnected() ? "connected" : "disconnected";
    } else if (component == "memory") {
        return mockGetMemoryError() ? "error" : "ok";
    } else if (component == "security") {
        return mockGetAccessGranted() ? "granted" : "denied";
    }
    return "unknown";
}

bool mockVerifyStateConsistency() {
    return mockGetNetworkConnected() && !mockGetMemoryError() && mockGetAccessGranted();
}

void mockSimulateError(const std::string& component, const std::string& error) {
    mockAddNetworkError(component + ": " + error);
}

std::vector<std::string> mockGetActiveErrors() {
    std::vector<std::string> errors;
    while (mockHasNetworkErrors()) {
        errors.push_back(mockGetNextNetworkError());
    }
    return errors;
} 