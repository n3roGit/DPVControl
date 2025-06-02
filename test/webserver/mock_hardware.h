#pragma once

#include <cstdint>
#include <string>
#include <array>
#include <vector>
#include <queue>
#include <mutex>

// Mock hardware states
extern int mockLEDState;
extern int mockMotorStep;
extern bool mockBeeperEnabled;
extern bool mockWaterSensorFront;
extern bool mockWaterSensorBack;
extern bool mockLeftButton;
extern bool mockRightButton;

// Mock LED bar states (20 LEDs total, split into two sections)
extern std::array<bool, 20> mockLEDBarStates;
extern int mockLEDBarBrightness;
extern int mockLEDBarSection1Brightness;
extern int mockLEDBarSection2Brightness;

// Mock front lamp PWM state
extern int mockFrontLampPWM;
extern int mockFrontLampFrequency;
extern int mockFrontLampResolution;

// Mock click code states
extern std::vector<int> mockClickCode;
extern bool mockClickCodeActive;
extern int mockClickCodeTimeout;

// Mock network states
extern bool mockNetworkConnected;
extern int mockNetworkLatency;
extern int mockNetworkErrorRate;
extern std::queue<std::string> mockNetworkErrors;

// Mock memory states
extern size_t mockFreeHeap;
extern size_t mockTotalHeap;
extern size_t mockMaxAllocation;
extern bool mockMemoryError;

// Mock security states
extern bool mockAccessGranted;
extern int mockRateLimit;
extern int mockRequestCount;
extern std::vector<std::string> mockInvalidInputs;

// Mock performance states
extern int mockProcessingTime;
extern int mockConcurrentRequests;
extern size_t mockDataSize;

// Mock hardware functions
void mockSetLEDState(int state);
int mockGetLEDState();
void mockSetMotorStep(int step);
int mockGetMotorStep();
void mockSetBeeperEnabled(bool enabled);
bool mockGetBeeperEnabled();
void mockSetWaterSensorFront(bool state);
bool mockGetWaterSensorFront();
void mockSetWaterSensorBack(bool state);
bool mockGetWaterSensorBack();
void mockSetLeftButton(bool state);
bool mockGetLeftButton();
void mockSetRightButton(bool state);
bool mockGetRightButton();

// Mock front lamp PWM functions
void mockSetFrontLampPWM(int value);
int mockGetFrontLampPWM();
void mockSetFrontLampFrequency(int frequency);
int mockGetFrontLampFrequency();
void mockSetFrontLampResolution(int resolution);
int mockGetFrontLampResolution();
void mockSetFrontLampDutyCycle(float dutyCycle);
float mockGetFrontLampDutyCycle();

// Mock LED bar functions
void mockSetLEDBarState(int index, bool state);
bool mockGetLEDBarState(int index);
void mockSetLEDBarBrightness(int brightness);
int mockGetLEDBarBrightness();
void mockSetLEDBarSectionBrightness(int section, int brightness);
int mockGetLEDBarSectionBrightness(int section);
void mockSetLEDBarPattern(const std::array<bool, 20>& pattern);
std::array<bool, 20> mockGetLEDBarPattern();
void mockSetLEDBarSectionPattern(int section, const std::array<bool, 10>& pattern);
std::array<bool, 10> mockGetLEDBarSectionPattern(int section);
void mockClearLEDBar();
void mockClearLEDBarSection(int section);
void mockSetAllLEDBar(bool state);
void mockSetAllLEDBarSection(int section, bool state);

// Mock click code functions
void mockSetClickCode(const std::vector<int>& code);
std::vector<int> mockGetClickCode();
void mockSetClickCodeActive(bool active);
bool mockGetClickCodeActive();
void mockSetClickCodeTimeout(int timeout);
int mockGetClickCodeTimeout();
void mockAddClick(int duration);
void mockClearClickCode();
bool mockVerifyClickCode(const std::vector<int>& code);

// Mock network functions
void mockSetNetworkConnected(bool connected);
bool mockGetNetworkConnected();
void mockSetNetworkLatency(int latency);
int mockGetNetworkLatency();
void mockSetNetworkErrorRate(int rate);
int mockGetNetworkErrorRate();
void mockAddNetworkError(const std::string& error);
std::string mockGetNextNetworkError();
bool mockHasNetworkErrors();

// Mock memory functions
void mockSetFreeHeap(size_t size);
size_t mockGetFreeHeap();
void mockSetTotalHeap(size_t size);
size_t mockGetTotalHeap();
void mockSetMaxAllocation(size_t size);
size_t mockGetMaxAllocation();
void mockSetMemoryError(bool error);
bool mockGetMemoryError();
bool mockAllocateMemory(size_t size);
void mockFreeMemory(size_t size);

// Mock security functions
void mockSetAccessGranted(bool granted);
bool mockGetAccessGranted();
void mockSetRateLimit(int limit);
int mockGetRateLimit();
void mockResetRequestCount();
int mockGetRequestCount();
void mockAddInvalidInput(const std::string& input);
bool mockIsInputValid(const std::string& input);
void mockClearInvalidInputs();

// Mock performance functions
void mockSetProcessingTime(int time);
int mockGetProcessingTime();
void mockSetConcurrentRequests(int count);
int mockGetConcurrentRequests();
void mockSetDataSize(size_t size);
size_t mockGetDataSize();
void mockSimulateLoad(int duration);
void mockResetPerformance();

// Mock integration functions
void mockResetAllStates();
void mockSimulateStateChange(const std::string& component, const std::string& state);
std::string mockGetComponentState(const std::string& component);
bool mockVerifyStateConsistency();
void mockSimulateError(const std::string& component, const std::string& error);
std::vector<std::string> mockGetActiveErrors();

#endif // MOCK_HARDWARE_H 