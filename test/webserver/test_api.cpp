#include <unity.h>
#include "mock_hardware.h"
#include "mock_webserver.h"
#include "Arduino.h"
#include "SPIFFS.h"
#include <string>

// Setup/Teardown
void setUp() {
    mock_resetAllStates();
}
void tearDown() {
    mock_resetAllStates();
}

// Beispiel-Testfall (weitere folgen analog)
void test_status_endpoint_returns_expected_keys() {
    std::string response = mockHandleApiStatus();
    TEST_ASSERT_NOT_NULL(strstr(response.c_str(), "temperature"));
    TEST_ASSERT_NOT_NULL(strstr(response.c_str(), "humidity"));
    TEST_ASSERT_NOT_NULL(strstr(response.c_str(), "motorSpeed"));
}

// ... weitere Testfälle für alle Endpunkte und Hardware ...

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_status_endpoint_returns_expected_keys);
    // ... weitere RUN_TEST-Aufrufe ...
    return UNITY_END();
} 