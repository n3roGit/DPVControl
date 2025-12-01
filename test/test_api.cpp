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
  void print(const char* str) { response = response + String(str); }
  void println(const char* str) { response = response + String(str) + "\n"; }
};

void setUp(void) { mockResponse = ""; }

void tearDown(void) { mockResponse = ""; }

void test_api_status() {
  handleApiStatus();
  JsonDocument doc;
  deserializeJson(doc, mockResponse);
  TEST_ASSERT_TRUE(doc["status"].is<const char*>());
  TEST_ASSERT_TRUE(doc["motor"].is<bool>());
  TEST_ASSERT_TRUE(doc["lamp"].is<bool>());
  TEST_ASSERT_TRUE(doc["beeper"].is<bool>());

  // Check new uptime fields
  TEST_ASSERT_TRUE_MESSAGE(doc.containsKey("uptime"), "Missing uptime field in status API");
  TEST_ASSERT_TRUE_MESSAGE(doc.containsKey("totalUptime"), "Missing totalUptime field in status API");
  TEST_ASSERT_TRUE(doc["uptime"].is<unsigned long>());
  TEST_ASSERT_TRUE(doc["totalUptime"].is<unsigned long>());
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

void test_status_api_uptime_fields() {
  // Test that status API includes uptime fields (prevents bug where uptime was
  // missing)
  setMockMillis(60000);  // 1 minute uptime
  bootTimeSeconds = 0;

  handleApiStatus();

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, mockResponse);
  TEST_ASSERT_FALSE(error);

  // Check that uptime fields exist (this would have caught the missing uptime
  // bug)
  TEST_ASSERT_TRUE_MESSAGE(doc.containsKey("uptime"), "Status API missing uptime field");
  TEST_ASSERT_TRUE_MESSAGE(doc.containsKey("totalUptime"), "Status API missing totalUptime field");

  // Check that they are numeric
  TEST_ASSERT_TRUE(doc["uptime"].is<unsigned long>());
  TEST_ASSERT_TRUE(doc["totalUptime"].is<unsigned long>());
}

void test_status_api_all_required_fields() {
  // Test that all expected status fields are present
  handleApiStatus();

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, mockResponse);
  TEST_ASSERT_FALSE(error);

  // Required fields that frontend expects
  const char* requiredFields[] = {"status", "uptime", "totalUptime", "motor", "lamp", "beeper", "erpm", "leftButton", "rightButton"};

  for (int i = 0; i < 9; i++) {
    TEST_ASSERT_TRUE_MESSAGE(doc.containsKey(requiredFields[i]), ("Missing required field: " + String(requiredFields[i])).c_str());
  }
}

void test_api_responses_are_valid_json() {
  // Test that all API endpoints return valid JSON (prevents malformed
  // responses)

  // Test status API
  handleApiStatus();
  JsonDocument statusDoc;
  TEST_ASSERT_FALSE(deserializeJson(statusDoc, mockResponse));

  mockResponse = "";

  // Test motor API
  handleApiMotor();
  JsonDocument motorDoc;
  TEST_ASSERT_FALSE(deserializeJson(motorDoc, mockResponse));

  mockResponse = "";

  // Test lamp API
  handleApiLamp();
  JsonDocument lampDoc;
  TEST_ASSERT_FALSE(deserializeJson(lampDoc, mockResponse));

  mockResponse = "";

  // Test version API
  handleApiVersion();
  JsonDocument versionDoc;
  TEST_ASSERT_FALSE(deserializeJson(versionDoc, mockResponse));

  mockResponse = "";

  // Test VESC bridge API
  handleApiVescBridge();
  JsonDocument bridgeDoc;
  TEST_ASSERT_FALSE(deserializeJson(bridgeDoc, mockResponse));
  TEST_ASSERT_TRUE(bridgeDoc["supported"].is<bool>());
  TEST_ASSERT_TRUE(bridgeDoc["active"].is<bool>());
  TEST_ASSERT_TRUE(bridgeDoc["mode"].is<const char*>());
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_api_status);
  RUN_TEST(test_api_motor);
  RUN_TEST(test_api_lamp);
  RUN_TEST(test_api_beeper);
  RUN_TEST(test_api_settings);
  RUN_TEST(test_api_version);
  RUN_TEST(test_status_api_uptime_fields);
  RUN_TEST(test_status_api_all_required_fields);
  RUN_TEST(test_api_responses_are_valid_json);
  UNITY_END();
  return 0;
} 