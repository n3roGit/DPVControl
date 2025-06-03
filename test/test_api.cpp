#define ARDUINOJSON_ENABLE_PROGMEM 0
#define ARDUINOJSON_ENABLE_FLASH_STRING 0
#include <unity.h>
#include "ArduinoJson_config.h"
#include "ArduinoJson.h"
#include "mock_arduino.h"
#include "mock_spiffs.h"
#include "mock_dhtesp.h"
#include "mock_webserver.h"
#include "mock_hardware.h"
#include "../src/webserver.h"

// Mock client for testing
class MockClient {
public:
    String response;
    String method;
    String url;
    
    void setMethod(const char* m) { method = m; }
    void setUrl(const char* u) { url = u; }
    void print(const char* str) { 
        response = response + String(str); 
    }
    void println(const char* str) { 
        response = response + String(str) + "\n"; 
    }
};

void setUp(void) {
    mockResponse = "";
}

void tearDown(void) {
    mockResponse = "";
}

void test_api_status() {
    handleApiStatus();
    JsonDocument doc;
    deserializeJson(doc, mockResponse);
    TEST_ASSERT_TRUE(doc["status"].is<const char*>());
    TEST_ASSERT_TRUE(doc["motor"].is<bool>());
    TEST_ASSERT_TRUE(doc["lamp"].is<bool>());
    TEST_ASSERT_TRUE(doc["beeper"].is<bool>());
}

void test_api_motor() {
    handleApiMotor();
    TEST_ASSERT_EQUAL_STRING("{\"success\":true}", mockResponse.c_str());
}

void test_api_lamp() {
    handleApiLamp();
    TEST_ASSERT_EQUAL_STRING("{\"success\":true}", mockResponse.c_str());
}

void test_api_beeper() {
    handleApiBeeper();
    TEST_ASSERT_EQUAL_STRING("{\"success\":true}", mockResponse.c_str());
}

void test_api_settings() {
    handleApiSettings();
    JsonDocument doc;
    deserializeJson(doc, mockResponse);
    TEST_ASSERT_TRUE(doc["motorEnabled"].is<bool>());
    TEST_ASSERT_TRUE(doc["lampEnabled"].is<bool>());
    TEST_ASSERT_TRUE(doc["beeperEnabled"].is<bool>());
    TEST_ASSERT_TRUE(doc["motorDuration"].is<int>());
    TEST_ASSERT_TRUE(doc["lampDuration"].is<int>());
    TEST_ASSERT_TRUE(doc["beeperDuration"].is<int>());
}

void test_api_version() {
    handleApiVersion();
    JsonDocument doc;
    deserializeJson(doc, mockResponse);
    TEST_ASSERT_TRUE(doc["version"].is<const char*>());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_api_status);
    RUN_TEST(test_api_motor);
    RUN_TEST(test_api_lamp);
    RUN_TEST(test_api_beeper);
    RUN_TEST(test_api_settings);
    RUN_TEST(test_api_version);
    UNITY_END();
    return 0;
} 