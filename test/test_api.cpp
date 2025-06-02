#include <unity.h>
#include <ArduinoJson.h>
#include "mock_arduino.h"
#include "mock_spiffs.h"
#include "../src/webserver.h"

// Mock client for testing
class MockClient {
public:
    String response;
    String method;
    String url;
    
    void setMethod(const char* m) { method = m; }
    void setUrl(const char* u) { url = u; }
    void print(const char* str) { response += str; }
    void println(const char* str) { response += str; response += "\n"; }
};

void setUp(void) {
    // Setup code
}

void tearDown(void) {
    // Cleanup code
}

void test_api_status() {
    MockClient client;
    client.setMethod("GET");
    client.setUrl("/api/status");
    
    handleApiStatus(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    TEST_ASSERT(doc["uptime"].is<unsigned long>());
    TEST_ASSERT(doc["totalUptime"].is<unsigned long>());
    TEST_ASSERT(doc["dataPoints"].is<int>());
    TEST_ASSERT(doc["beeperEnabled"].is<bool>());
    TEST_ASSERT(doc["lampLevel"].is<int>());
    TEST_ASSERT(doc["waterSensorFront"].is<bool>());
    TEST_ASSERT(doc["waterSensorBack"].is<bool>());
    TEST_ASSERT(doc["leftButton"].is<bool>());
    TEST_ASSERT(doc["rightButton"].is<bool>());
}

void test_api_motor() {
    MockClient client;
    client.setMethod("POST");
    client.setUrl("/api/motor");
    
    handleApiMotor(&client);
    
    TEST_ASSERT_EQUAL_STRING("{\"success\":true}", client.response.c_str());
}

void test_api_lamp() {
    MockClient client;
    client.setMethod("POST");
    client.setUrl("/api/lamp");
    
    handleApiLamp(&client);
    
    TEST_ASSERT_EQUAL_STRING("{\"success\":true}", client.response.c_str());
}

void test_api_beeper() {
    MockClient client;
    client.setMethod("POST");
    client.setUrl("/api/beeper");
    
    handleApiBeeper(&client);
    
    TEST_ASSERT_EQUAL_STRING("{\"success\":true}", client.response.c_str());
}

void test_api_settings() {
    MockClient client;
    client.setMethod("GET");
    client.setUrl("/api/settings");
    
    handleApiSettings(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    TEST_ASSERT(doc["speedSteps"].is<int>());
    TEST_ASSERT(doc["standbyDelaySeconds"].is<int>());
    TEST_ASSERT(doc["batteryPowerMax"].is<int>());
    TEST_ASSERT(doc["minSpeedPercent"].is<int>());
    TEST_ASSERT(doc["maxSpeedRpm"].is<int>());
    TEST_ASSERT(doc["ledBarNum"].is<int>());
    TEST_ASSERT(doc["lampMaxLevels"].is<int>());
    TEST_ASSERT(doc["beeperEnabled"].is<bool>());
}

void test_api_version() {
    MockClient client;
    client.setMethod("GET");
    client.setUrl("/api/version");
    
    handleApiVersion(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    TEST_ASSERT(doc["version"].is<const char*>());
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