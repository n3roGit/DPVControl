#include <gtest/gtest.h>
#include "mock_hardware.h"
#include "webserver.h"
#include <ArduinoJson.h>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <memory>

// Mock WiFiClient for testing
class MockWiFiClient {
public:
    std::string response;
    std::string requestBody;
    
    void print(const String& data) {
        requestBody = data.c_str();
    }
    
    String readString() {
        return String(response.c_str());
    }
    
    bool connected() { return true; }
    bool available() { return true; }
    char read() { return 0; }
    void stop() {}
};

// Mock sendHttpResponse function
void sendHttpResponse(void* client, int statusCode, const char* contentType, const char* content) {
    MockWiFiClient* mockClient = static_cast<MockWiFiClient*>(client);
    mockClient->response = content;
}

// Mock session data structure
struct MockSession {
    String id;
    String timestamp;
    std::vector<LogdataRow> data;
};

class WebApiTest : public ::testing::Test {
protected:
    MockWiFiClient client;
    std::vector<MockSession> mockSessions;
    
    void SetUp() override {
        mockResetAllStates();
        createTestSessions();
    }

    void TearDown() override {
        mockResetAllStates();
    }

    // Helper methods for common test operations
    void verifyJsonResponse(const std::string& response, const std::vector<std::string>& requiredKeys) {
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, response);
        
        for (const auto& key : requiredKeys) {
            EXPECT_TRUE(doc.containsKey(key.c_str())) << "Missing key: " << key;
        }
    }

    void verifyErrorResponse(const std::string& response, const std::string& expectedError) {
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, response);
        
        EXPECT_TRUE(doc.containsKey("error"));
        EXPECT_EQ(doc["error"].as<std::string>(), expectedError);
    }

    void verifySuccessResponse(const std::string& response) {
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, response);
        
        EXPECT_TRUE(doc["success"].as<bool>());
    }

    void createTestSessions() {
        mockSessions.clear();
        
        // Create test session with realistic data
        MockSession session;
        session.id = "test_session_1";
        session.timestamp = "2024-03-20T10:00:00";
        
        // Add data points with realistic values
        for (int i = 0; i < 10; i++) {
            LogdataRow data;
            data.timestamp = 1710921600 + (i * 60); // 1 minute intervals
            data.motorSpeed = 50 + (i * 5);         // Increasing speed
            data.batteryVoltage = 14.8 - (i * 0.1); // Decreasing voltage
            data.temperature = 25.5 + (i * 0.5);    // Increasing temperature
            session.data.push_back(data);
        }
        
        mockSessions.push_back(session);
    }
};

// Test session storage and retrieval
TEST_F(WebApiTest, SessionStorage) {
    // Test storing a new session
    String sessionData = "{\"timestamp\":\"2024-03-20T12:00:00\",\"data\":[{\"motorSpeed\":50,\"batteryVoltage\":14.8,\"temperature\":25.5}]}";
    client.print(sessionData);
    
    handleApiSessionStore(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc["success"].as<bool>());
    EXPECT_TRUE(doc.containsKey("sessionId"));
    
    // Test retrieving the stored session
    String sessionId = doc["sessionId"].as<String>();
    handleApiSessionData(&client, sessionId.c_str());
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc.containsKey("session"));
    EXPECT_TRUE(doc.containsKey("data"));
    EXPECT_TRUE(doc["data"].is<JsonArray>());
    EXPECT_EQ(doc["data"][0]["motorSpeed"].as<int>(), 50);
    EXPECT_EQ(doc["data"][0]["batteryVoltage"].as<float>(), 14.8);
    EXPECT_EQ(doc["data"][0]["temperature"].as<float>(), 25.5);
}

// Test session export to CSV
TEST_F(WebApiTest, SessionExport) {
    // Test exporting session 1 to CSV
    handleApiSessionCsv(&client, "test_session_1");
    
    // Verify CSV format
    std::string csv = client.response;
    
    // Check headers
    EXPECT_TRUE(csv.find("timestamp") != std::string::npos);
    EXPECT_TRUE(csv.find("motor_speed") != std::string::npos);
    EXPECT_TRUE(csv.find("battery_voltage") != std::string::npos);
    EXPECT_TRUE(csv.find("temperature") != std::string::npos);
    
    // Check data rows
    EXPECT_TRUE(csv.find("2024-03-20T10:00:00") != std::string::npos);
    EXPECT_TRUE(csv.find("50") != std::string::npos);
    EXPECT_TRUE(csv.find("14.8") != std::string::npos);
    EXPECT_TRUE(csv.find("25.5") != std::string::npos);
    
    // Test exporting session 2 to CSV
    client = MockWiFiClient();
    handleApiSessionCsv(&client, "test_session_2");
    
    csv = client.response;
    
    // Check data rows
    EXPECT_TRUE(csv.find("2024-03-20T11:00:00") != std::string::npos);
    EXPECT_TRUE(csv.find("100") != std::string::npos);
    EXPECT_TRUE(csv.find("14.6") != std::string::npos);
    EXPECT_TRUE(csv.find("26.5") != std::string::npos);
}

// Test session list with multiple sessions
TEST_F(WebApiTest, SessionList) {
    handleApiSessions(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc.containsKey("sessions"));
    EXPECT_TRUE(doc["sessions"].is<JsonArray>());
    
    JsonArray sessions = doc["sessions"];
    EXPECT_EQ(sessions.size(), 2);
    
    // Check first session
    EXPECT_EQ(sessions[0]["id"].as<String>(), "test_session_1");
    EXPECT_EQ(sessions[0]["timestamp"].as<String>(), "2024-03-20T10:00:00");
    EXPECT_EQ(sessions[0]["dataPoints"].as<int>(), 2);
    
    // Check second session
    EXPECT_EQ(sessions[1]["id"].as<String>(), "test_session_2");
    EXPECT_EQ(sessions[1]["timestamp"].as<String>(), "2024-03-20T11:00:00");
    EXPECT_EQ(sessions[1]["dataPoints"].as<int>(), 1);
}

// Test session data retrieval with pagination
TEST_F(WebApiTest, SessionDataPagination) {
    // Test retrieving first page
    handleApiSessionData(&client, "test_session_1?page=1&limit=1");
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc.containsKey("session"));
    EXPECT_TRUE(doc.containsKey("data"));
    EXPECT_TRUE(doc["data"].is<JsonArray>());
    EXPECT_EQ(doc["data"].size(), 1);
    EXPECT_EQ(doc["data"][0]["motorSpeed"].as<int>(), 50);
    
    // Test retrieving second page
    client = MockWiFiClient();
    handleApiSessionData(&client, "test_session_1?page=2&limit=1");
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc.containsKey("session"));
    EXPECT_TRUE(doc.containsKey("data"));
    EXPECT_TRUE(doc["data"].is<JsonArray>());
    EXPECT_EQ(doc["data"].size(), 1);
    EXPECT_EQ(doc["data"][0]["motorSpeed"].as<int>(), 75);
}

// Test session data retrieval with date range
TEST_F(WebApiTest, SessionDataDateRange) {
    // Test retrieving data within date range
    handleApiSessionData(&client, "test_session_1?start=2024-03-20T09:59:00&end=2024-03-20T10:01:00");
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc.containsKey("session"));
    EXPECT_TRUE(doc.containsKey("data"));
    EXPECT_TRUE(doc["data"].is<JsonArray>());
    EXPECT_EQ(doc["data"].size(), 2);
    
    // Test retrieving data outside date range
    client = MockWiFiClient();
    handleApiSessionData(&client, "test_session_1?start=2024-03-20T11:00:00&end=2024-03-20T12:00:00");
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc.containsKey("session"));
    EXPECT_TRUE(doc.containsKey("data"));
    EXPECT_TRUE(doc["data"].is<JsonArray>());
    EXPECT_EQ(doc["data"].size(), 0);
}

// Test /api/status endpoint
TEST_F(WebApiTest, StatusEndpoint_ReturnsAllRequiredFields) {
    // Arrange
    mockSetUptime(3600);
    mockSetTotalUptime(7200);
    mockSetDataPoints(100);
    
    // Act
    handleApiStatus(&client);
    
    // Assert
    verifyJsonResponse(client.response, {
        "uptime", "totalUptime", "dataPoints", "beeperEnabled",
        "lampLevel", "waterSensorFront", "waterSensorBack",
        "leftButton", "rightButton", "batteryVoltage",
        "temperature", "motorSpeed", "systemStatus"
    });
}

TEST_F(WebApiTest, StatusEndpoint_HandlesDisconnectedClient) {
    // Arrange
    client.disconnect();
    
    // Act
    handleApiStatus(&client);
    
    // Assert
    verifyErrorResponse(client.response, "Client disconnected");
}

// Test /api/data endpoint
TEST_F(WebApiTest, DataEndpoint) {
    // Test without parameters
    handleApiData(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc.containsKey("data"));
    EXPECT_TRUE(doc["data"].is<JsonArray>());
    
    // Test with count parameter
    client = MockWiFiClient();
    handleApiData(&client, "?count=50");
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc.containsKey("data"));
    EXPECT_TRUE(doc["data"].is<JsonArray>());
    EXPECT_LE(doc["data"].size(), 50);
    
    // Test with range parameter
    client = MockWiFiClient();
    handleApiData(&client, "?range=day");
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc.containsKey("data"));
    EXPECT_TRUE(doc["data"].is<JsonArray>());
}

// Test /api/data endpoint error cases
TEST_F(WebApiTest, DataEndpointErrors) {
    // Test invalid count (too large)
    handleApiData(&client, "?count=1000");
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc.containsKey("error"));
    
    // Test invalid count (negative)
    client = MockWiFiClient();
    handleApiData(&client, "?count=-1");
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc.containsKey("error"));
    
    // Test invalid range
    client = MockWiFiClient();
    handleApiData(&client, "?range=invalid");
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc.containsKey("error"));
}

// Test /api/sessions endpoint
TEST_F(WebApiTest, SessionsEndpoint) {
    handleApiSessions(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc.containsKey("sessions"));
    EXPECT_TRUE(doc["sessions"].is<JsonArray>());
}

// Test /api/sessions/{id}/data endpoint
TEST_F(WebApiTest, SessionDataEndpoint) {
    handleApiSessionData(&client, "test_session");
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc.containsKey("session"));
    EXPECT_TRUE(doc.containsKey("data"));
    EXPECT_TRUE(doc["data"].is<JsonArray>());
}

// Test /api/sessions/{id}/data endpoint error cases
TEST_F(WebApiTest, SessionDataEndpointErrors) {
    // Test non-existent session
    handleApiSessionData(&client, "non_existent_session");
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc.containsKey("error"));
    
    // Test invalid session ID
    client = MockWiFiClient();
    handleApiSessionData(&client, "invalid/session/id");
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc.containsKey("error"));
}

// Test /api/sessions/{id}/csv endpoint
TEST_F(WebApiTest, SessionCsvEndpoint) {
    handleApiSessionCsv(&client, "test_session");
    
    // CSV response should start with headers
    EXPECT_TRUE(client.response.find("timestamp") != std::string::npos);
    EXPECT_TRUE(client.response.find("motor_speed") != std::string::npos);
}

// Test /api/sessions/{id}/csv endpoint error cases
TEST_F(WebApiTest, SessionCsvEndpointErrors) {
    // Test non-existent session
    handleApiSessionCsv(&client, "non_existent_session");
    
    EXPECT_TRUE(client.response.find("error") != std::string::npos);
    
    // Test invalid session ID
    client = MockWiFiClient();
    handleApiSessionCsv(&client, "invalid/session/id");
    
    EXPECT_TRUE(client.response.find("error") != std::string::npos);
}

// Test /api/motor endpoint
TEST_F(WebApiTest, MotorEndpoint) {
    String body = "{\"enabled\":true,\"speed\":50}";
    client.print(body);
    
    handleApiMotor(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc["success"].as<bool>());
    EXPECT_TRUE(doc["enabled"].as<bool>());
    EXPECT_EQ(doc["speed"].as<int>(), 50);
    EXPECT_EQ(doc["motorStep"].as<int>(), mockGetMotorStep());
}

// Test /api/motor endpoint with invalid JSON
TEST_F(WebApiTest, MotorEndpointInvalidJson) {
    String body = "invalid json";
    client.print(body);
    
    handleApiMotor(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_FALSE(doc["success"].as<bool>());
}

// Test /api/motor endpoint with invalid speed
TEST_F(WebApiTest, MotorEndpointInvalidSpeed) {
    String body = "{\"enabled\":true,\"speed\":-1}";
    client.print(body);
    
    handleApiMotor(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_FALSE(doc["success"].as<bool>());
}

// Test /api/motor endpoint with invalid speed
TEST_F(WebApiTest, MotorEndpointInvalidSpeed) {
    String body = "{\"enabled\":true,\"speed\":101}";
    client.print(body);
    
    handleApiMotor(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_FALSE(doc["success"].as<bool>());
}

// Test /api/motor endpoint with missing speed parameter
TEST_F(WebApiTest, MotorEndpointMissingSpeedParameter) {
    String body = "{\"enabled\":true}";
    client.print(body);
    
    handleApiMotor(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_FALSE(doc["success"].as<bool>());
}

// Test /api/motor endpoint with missing enabled parameter
TEST_F(WebApiTest, MotorEndpointMissingEnabledParameter) {
    String body = "{\"speed\":50}";
    client.print(body);
    
    handleApiMotor(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_FALSE(doc["success"].as<bool>());
}

// Test /api/motor endpoint with invalid JSON
TEST_F(WebApiTest, MotorEndpointInvalidJson) {
    String body = "invalid json";
    client.print(body);
    
    handleApiMotor(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_FALSE(doc["success"].as<bool>());
}

// Test /api/motor endpoint with water sensor interaction
TEST_F(WebApiTest, MotorEndpointWaterSensor) {
    // Arrange
    mockSetWaterSensorFront(true);
    client.print("{\"enabled\":true,\"speed\":50}");
    
    // Act
    handleApiMotor(&client);
    
    // Assert
    verifyErrorResponse(client.response, "Water sensor active");
    EXPECT_EQ(mockGetMotorStep(), 0);
}

// Test /api/lamp endpoint
TEST_F(WebApiTest, LampEndpoint) {
    String body = "{\"level\":75}";
    client.print(body);
    
    handleApiLamp(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc["success"].as<bool>());
    EXPECT_EQ(doc["level"].as<int>(), 75);
    EXPECT_EQ(mockGetLEDState(), 75);
}

// Test /api/lamp endpoint with invalid level
TEST_F(WebApiTest, LampEndpointInvalidLevel) {
    String body = "{\"level\":-1}";
    client.print(body);
    
    handleApiLamp(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_FALSE(doc["success"].as<bool>());
}

// Test /api/beeper endpoint
TEST_F(WebApiTest, BeeperEndpoint) {
    String body = "{\"enabled\":true}";
    client.print(body);
    
    handleApiBeeper(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc["success"].as<bool>());
    EXPECT_TRUE(doc["enabled"].as<bool>());
    EXPECT_TRUE(mockGetBeeperEnabled());
}

// Test /api/settings endpoint
TEST_F(WebApiTest, SettingsEndpoint) {
    // Test GET
    handleApiSettings(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc.containsKey("beeperEnabled"));
    EXPECT_TRUE(doc.containsKey("lampLevel"));
    EXPECT_TRUE(doc.containsKey("motorSpeed"));
    
    // Test POST
    client = MockWiFiClient();
    String body = "{\"beeperEnabled\":true,\"lampLevel\":50,\"motorSpeed\":75}";
    client.print(body);
    
    handleApiSettings(&client);
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc["success"].as<bool>());
    EXPECT_TRUE(mockGetBeeperEnabled());
    EXPECT_EQ(mockGetLEDState(), 50);
}

// Test /api/settings endpoint with invalid settings
TEST_F(WebApiTest, SettingsEndpointInvalidSettings) {
    String body = "{\"beeperEnabled\":true,\"lampLevel\":999,\"motorSpeed\":-1}";
    client.print(body);
    
    handleApiSettings(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_FALSE(doc["success"].as<bool>());
}

// Test /api/settings/restore endpoint
TEST_F(WebApiTest, SettingsRestoreEndpoint) {
    handleApiSettingsRestore(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc["success"].as<bool>());
}

// Test /api/settings/restore endpoint error case
TEST_F(WebApiTest, SettingsRestoreEndpointError) {
    // Simulate restore failure
    mockSetRestoreError(true);
    
    handleApiSettingsRestore(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_FALSE(doc["success"].as<bool>());
    EXPECT_TRUE(doc.containsKey("error"));
    
    mockSetRestoreError(false);
}

// Test /api/reboot endpoint
TEST_F(WebApiTest, RebootEndpoint) {
    handleApiReboot(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc["success"].as<bool>());
    EXPECT_TRUE(doc.containsKey("message"));
}

// Test /api/delete-all-sessions endpoint
TEST_F(WebApiTest, DeleteAllSessionsEndpoint) {
    handleApiDeleteAllSessions(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc["success"].as<bool>());
    EXPECT_TRUE(doc.containsKey("deleted"));
    EXPECT_TRUE(doc.containsKey("message"));
}

// Test /api/delete-all-sessions endpoint error cases
TEST_F(WebApiTest, DeleteAllSessionsEndpointErrors) {
    // Test with no sessions
    mockSetSessionCount(0);
    
    handleApiDeleteAllSessions(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc["success"].as<bool>());
    EXPECT_EQ(doc["deleted"].as<int>(), 0);
    
    // Test with delete error
    mockSetSessionCount(5);
    mockSetDeleteError(true);
    
    client = MockWiFiClient();
    handleApiDeleteAllSessions(&client);
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_FALSE(doc["success"].as<bool>());
    EXPECT_TRUE(doc.containsKey("error"));
    
    mockSetDeleteError(false);
}

// Test /api/version endpoint
TEST_F(WebApiTest, VersionEndpoint) {
    handleApiVersion(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc.containsKey("version"));
    EXPECT_FALSE(doc["version"].as<String>().isEmpty());
}

// Test chart data generation
TEST_F(WebApiTest, ChartData) {
    // Test chart data for session 1
    handleApiSessionData(&client, "test_session_1?format=chart");
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc.containsKey("chartData"));
    EXPECT_TRUE(doc["chartData"].is<JsonObject>());
    
    // Check chart data structure
    JsonObject chartData = doc["chartData"];
    EXPECT_TRUE(chartData.containsKey("labels"));
    EXPECT_TRUE(chartData.containsKey("datasets"));
    
    // Check labels (timestamps)
    JsonArray labels = chartData["labels"];
    EXPECT_EQ(labels.size(), 2);
    EXPECT_EQ(labels[0].as<String>(), "2024-03-20T10:00:00");
    EXPECT_EQ(labels[1].as<String>(), "2024-03-20T10:01:00");
    
    // Check datasets
    JsonArray datasets = chartData["datasets"];
    EXPECT_EQ(datasets.size(), 3); // motorSpeed, batteryVoltage, temperature
    
    // Check motor speed dataset
    JsonObject motorDataset = datasets[0];
    EXPECT_EQ(motorDataset["label"].as<String>(), "Motor Speed");
    EXPECT_EQ(motorDataset["data"][0].as<int>(), 50);
    EXPECT_EQ(motorDataset["data"][1].as<int>(), 75);
    
    // Check battery voltage dataset
    JsonObject batteryDataset = datasets[1];
    EXPECT_EQ(batteryDataset["label"].as<String>(), "Battery Voltage");
    EXPECT_EQ(batteryDataset["data"][0].as<float>(), 14.8);
    EXPECT_EQ(batteryDataset["data"][1].as<float>(), 14.7);
    
    // Check temperature dataset
    JsonObject tempDataset = datasets[2];
    EXPECT_EQ(tempDataset["label"].as<String>(), "Temperature");
    EXPECT_EQ(tempDataset["data"][0].as<float>(), 25.5);
    EXPECT_EQ(tempDataset["data"][1].as<float>(), 26.0);
}

// Test chart data with time range
TEST_F(WebApiTest, ChartDataTimeRange) {
    // Test chart data with time range
    handleApiSessionData(&client, "test_session_1?format=chart&start=2024-03-20T09:59:00&end=2024-03-20T10:01:00");
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc.containsKey("chartData"));
    JsonObject chartData = doc["chartData"];
    
    // Check if data is filtered correctly
    JsonArray labels = chartData["labels"];
    EXPECT_EQ(labels.size(), 2);
    
    JsonArray datasets = chartData["datasets"];
    JsonObject motorDataset = datasets[0];
    EXPECT_EQ(motorDataset["data"].size(), 2);
}

// Test chart data with empty time range
TEST_F(WebApiTest, ChartDataEmptyTimeRange) {
    // Test chart data with empty time range
    handleApiSessionData(&client, "test_session_1?format=chart&start=2024-03-20T11:00:00&end=2024-03-20T12:00:00");
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc.containsKey("chartData"));
    JsonObject chartData = doc["chartData"];
    
    // Check if data is empty
    JsonArray labels = chartData["labels"];
    EXPECT_EQ(labels.size(), 0);
    
    JsonArray datasets = chartData["datasets"];
    JsonObject motorDataset = datasets[0];
    EXPECT_EQ(motorDataset["data"].size(), 0);
}

// Test chart data with invalid session
TEST_F(WebApiTest, ChartDataInvalidSession) {
    // Test chart data with non-existent session
    handleApiSessionData(&client, "non_existent_session?format=chart");
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc.containsKey("error"));
    EXPECT_FALSE(doc.containsKey("chartData"));
}

// Test chart data with invalid format
TEST_F(WebApiTest, ChartDataInvalidFormat) {
    // Test chart data with invalid format parameter
    handleApiSessionData(&client, "test_session_1?format=invalid");
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc.containsKey("error"));
    EXPECT_FALSE(doc.containsKey("chartData"));
}

// Test remote control functionality
TEST_F(WebApiTest, RemoteControl) {
    // Test motor control
    String motorBody = "{\"enabled\":true,\"speed\":75}";
    client.print(motorBody);
    
    handleApiMotor(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc["success"].as<bool>());
    EXPECT_TRUE(doc["enabled"].as<bool>());
    EXPECT_EQ(doc["speed"].as<int>(), 75);
    EXPECT_EQ(mockGetMotorStep(), 75);
    
    // Test lamp control
    client = MockWiFiClient();
    String lampBody = "{\"level\":50}";
    client.print(lampBody);
    
    handleApiLamp(&client);
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc["success"].as<bool>());
    EXPECT_EQ(doc["level"].as<int>(), 50);
    EXPECT_EQ(mockGetLEDState(), 50);
    
    // Test beeper control
    client = MockWiFiClient();
    String beeperBody = "{\"enabled\":true}";
    client.print(beeperBody);
    
    handleApiBeeper(&client);
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc["success"].as<bool>());
    EXPECT_TRUE(doc["enabled"].as<bool>());
    EXPECT_TRUE(mockGetBeeperEnabled());
}

// Test remote control error cases
TEST_F(WebApiTest, RemoteControlErrors) {
    // Test invalid motor speed
    String motorBody = "{\"enabled\":true,\"speed\":-1}";
    client.print(motorBody);
    
    handleApiMotor(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_FALSE(doc["success"].as<bool>());
    EXPECT_TRUE(doc.containsKey("error"));
    
    // Test invalid lamp level
    client = MockWiFiClient();
    String lampBody = "{\"level\":101}";
    client.print(lampBody);
    
    handleApiLamp(&client);
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_FALSE(doc["success"].as<bool>());
    EXPECT_TRUE(doc.containsKey("error"));
    
    // Test invalid JSON
    client = MockWiFiClient();
    String invalidBody = "invalid json";
    client.print(invalidBody);
    
    handleApiMotor(&client);
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_FALSE(doc["success"].as<bool>());
    EXPECT_TRUE(doc.containsKey("error"));
}

// Test remote control state persistence
TEST_F(WebApiTest, RemoteControlStatePersistence) {
    // Set initial states
    mockSetMotorStep(0);
    mockSetLEDState(0);
    mockSetBeeperEnabled(false);
    
    // Test motor state persistence
    String motorBody = "{\"enabled\":true,\"speed\":50}";
    client.print(motorBody);
    
    handleApiMotor(&client);
    
    EXPECT_EQ(mockGetMotorStep(), 50);
    
    // Test lamp state persistence
    client = MockWiFiClient();
    String lampBody = "{\"level\":75}";
    client.print(lampBody);
    
    handleApiLamp(&client);
    
    EXPECT_EQ(mockGetLEDState(), 75);
    
    // Test beeper state persistence
    client = MockWiFiClient();
    String beeperBody = "{\"enabled\":true}";
    client.print(beeperBody);
    
    handleApiBeeper(&client);
    
    EXPECT_TRUE(mockGetBeeperEnabled());
}

// Test remote control with water sensor interaction
TEST_F(WebApiTest, RemoteControlWaterSensor) {
    // Test motor control with water sensor active
    mockSetWaterSensorFront(true);
    
    String motorBody = "{\"enabled\":true,\"speed\":50}";
    client.print(motorBody);
    
    handleApiMotor(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_FALSE(doc["success"].as<bool>());
    EXPECT_TRUE(doc.containsKey("error"));
    EXPECT_EQ(mockGetMotorStep(), 0); // Motor should not start
    
    // Test lamp control with water sensor active
    client = MockWiFiClient();
    String lampBody = "{\"level\":50}";
    client.print(lampBody);
    
    handleApiLamp(&client);
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc["success"].as<bool>()); // Lamp should work
    EXPECT_EQ(mockGetLEDState(), 50);
}

// Test remote control with button interaction
TEST_F(WebApiTest, RemoteControlButton) {
    // Test motor control with button pressed
    mockSetLeftButton(true);
    
    String motorBody = "{\"enabled\":true,\"speed\":50}";
    client.print(motorBody);
    
    handleApiMotor(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_FALSE(doc["success"].as<bool>());
    EXPECT_TRUE(doc.containsKey("error"));
    EXPECT_EQ(mockGetMotorStep(), 0); // Motor should not start
    
    // Test lamp control with button pressed
    client = MockWiFiClient();
    String lampBody = "{\"level\":50}";
    client.print(lampBody);
    
    handleApiLamp(&client);
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc["success"].as<bool>()); // Lamp should work
    EXPECT_EQ(mockGetLEDState(), 50);
}

// Test settings page functionality
TEST_F(WebApiTest, SettingsPage) {
    // Test GET settings
    handleApiSettings(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc.containsKey("beeperEnabled"));
    EXPECT_TRUE(doc.containsKey("lampLevel"));
    EXPECT_TRUE(doc.containsKey("motorSpeed"));
    EXPECT_TRUE(doc.containsKey("waterSensorEnabled"));
    EXPECT_TRUE(doc.containsKey("buttonEnabled"));
    EXPECT_TRUE(doc.containsKey("autoShutdown"));
    EXPECT_TRUE(doc.containsKey("shutdownDelay"));
}

// Test settings update
TEST_F(WebApiTest, SettingsUpdate) {
    // Test valid settings update
    String settingsBody = R"({
        "beeperEnabled": true,
        "lampLevel": 75,
        "motorSpeed": 50,
        "waterSensorEnabled": true,
        "buttonEnabled": true,
        "autoShutdown": true,
        "shutdownDelay": 300
    })";
    client.print(settingsBody);
    
    handleApiSettings(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc["success"].as<bool>());
    EXPECT_TRUE(mockGetBeeperEnabled());
    EXPECT_EQ(mockGetLEDState(), 75);
    EXPECT_TRUE(mockGetWaterSensorEnabled());
    EXPECT_TRUE(mockGetButtonEnabled());
    EXPECT_TRUE(mockGetAutoShutdown());
    EXPECT_EQ(mockGetShutdownDelay(), 300);
}

// Test settings validation
TEST_F(WebApiTest, SettingsValidation) {
    // Test invalid lamp level
    String settingsBody = R"({
        "lampLevel": 101
    })";
    client.print(settingsBody);
    
    handleApiSettings(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_FALSE(doc["success"].as<bool>());
    EXPECT_TRUE(doc.containsKey("error"));
    
    // Test invalid motor speed
    client = MockWiFiClient();
    settingsBody = R"({
        "motorSpeed": -1
    })";
    client.print(settingsBody);
    
    handleApiSettings(&client);
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_FALSE(doc["success"].as<bool>());
    EXPECT_TRUE(doc.containsKey("error"));
    
    // Test invalid shutdown delay
    client = MockWiFiClient();
    settingsBody = R"({
        "shutdownDelay": -1
    })";
    client.print(settingsBody);
    
    handleApiSettings(&client);
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_FALSE(doc["success"].as<bool>());
    EXPECT_TRUE(doc.containsKey("error"));
}

// Test settings restore
TEST_F(WebApiTest, SettingsRestore) {
    // Test successful restore
    handleApiSettingsRestore(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc["success"].as<bool>());
    EXPECT_FALSE(mockGetBeeperEnabled());
    EXPECT_EQ(mockGetLEDState(), 0);
    EXPECT_TRUE(mockGetWaterSensorEnabled());
    EXPECT_TRUE(mockGetButtonEnabled());
    EXPECT_FALSE(mockGetAutoShutdown());
    EXPECT_EQ(mockGetShutdownDelay(), 0);
    
    // Test restore failure
    mockSetRestoreError(true);
    
    client = MockWiFiClient();
    handleApiSettingsRestore(&client);
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_FALSE(doc["success"].as<bool>());
    EXPECT_TRUE(doc.containsKey("error"));
    
    mockSetRestoreError(false);
}

// Test settings persistence
TEST_F(WebApiTest, SettingsPersistence) {
    // Set initial settings
    String settingsBody = R"({
        "beeperEnabled": true,
        "lampLevel": 50,
        "motorSpeed": 75,
        "waterSensorEnabled": true,
        "buttonEnabled": true,
        "autoShutdown": true,
        "shutdownDelay": 600
    })";
    client.print(settingsBody);
    
    handleApiSettings(&client);
    
    // Verify settings persistence
    client = MockWiFiClient();
    handleApiSettings(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc["beeperEnabled"].as<bool>());
    EXPECT_EQ(doc["lampLevel"].as<int>(), 50);
    EXPECT_EQ(doc["motorSpeed"].as<int>(), 75);
    EXPECT_TRUE(doc["waterSensorEnabled"].as<bool>());
    EXPECT_TRUE(doc["buttonEnabled"].as<bool>());
    EXPECT_TRUE(doc["autoShutdown"].as<bool>());
    EXPECT_EQ(doc["shutdownDelay"].as<int>(), 600);
}

// Test settings with water sensor interaction
TEST_F(WebApiTest, SettingsWaterSensor) {
    // Test settings update with water sensor disabled
    String settingsBody = R"({
        "waterSensorEnabled": false
    })";
    client.print(settingsBody);
    
    handleApiSettings(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc["success"].as<bool>());
    EXPECT_FALSE(mockGetWaterSensorEnabled());
    
    // Test motor control with water sensor disabled
    client = MockWiFiClient();
    mockSetWaterSensorFront(true);
    
    String motorBody = "{\"enabled\":true,\"speed\":50}";
    client.print(motorBody);
    
    handleApiMotor(&client);
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc["success"].as<bool>()); // Motor should work
    EXPECT_EQ(mockGetMotorStep(), 50);
}

// Test settings with button interaction
TEST_F(WebApiTest, SettingsButton) {
    // Test settings update with button disabled
    String settingsBody = R"({
        "buttonEnabled": false
    })";
    client.print(settingsBody);
    
    handleApiSettings(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc["success"].as<bool>());
    EXPECT_FALSE(mockGetButtonEnabled());
    
    // Test motor control with button pressed but disabled
    client = MockWiFiClient();
    mockSetLeftButton(true);
    
    String motorBody = "{\"enabled\":true,\"speed\":50}";
    client.print(motorBody);
    
    handleApiMotor(&client);
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc["success"].as<bool>()); // Motor should work
    EXPECT_EQ(mockGetMotorStep(), 50);
}

// Test status page functionality
TEST_F(WebApiTest, StatusPage) {
    // Test GET status
    handleApiStatus(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    // Check all required status fields
    EXPECT_TRUE(doc.containsKey("uptime"));
    EXPECT_TRUE(doc.containsKey("totalUptime"));
    EXPECT_TRUE(doc.containsKey("dataPoints"));
    EXPECT_TRUE(doc.containsKey("beeperEnabled"));
    EXPECT_TRUE(doc.containsKey("lampLevel"));
    EXPECT_TRUE(doc.containsKey("waterSensorFront"));
    EXPECT_TRUE(doc.containsKey("waterSensorBack"));
    EXPECT_TRUE(doc.containsKey("leftButton"));
    EXPECT_TRUE(doc.containsKey("rightButton"));
    EXPECT_TRUE(doc.containsKey("batteryVoltage"));
    EXPECT_TRUE(doc.containsKey("temperature"));
    EXPECT_TRUE(doc.containsKey("motorSpeed"));
    EXPECT_TRUE(doc.containsKey("systemStatus"));
}

// Test status values
TEST_F(WebApiTest, StatusValues) {
    // Set mock values
    mockSetUptime(3600);
    mockSetTotalUptime(7200);
    mockSetDataPoints(100);
    mockSetBeeperEnabled(true);
    mockSetLEDState(75);
    mockSetWaterSensorFront(true);
    mockSetWaterSensorBack(false);
    mockSetLeftButton(true);
    mockSetRightButton(false);
    mockSetBatteryVoltage(14.8);
    mockSetTemperature(25.5);
    mockSetMotorStep(50);
    mockSetSystemStatus("running");
    
    handleApiStatus(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    // Verify all status values
    EXPECT_EQ(doc["uptime"].as<int>(), 3600);
    EXPECT_EQ(doc["totalUptime"].as<int>(), 7200);
    EXPECT_EQ(doc["dataPoints"].as<int>(), 100);
    EXPECT_TRUE(doc["beeperEnabled"].as<bool>());
    EXPECT_EQ(doc["lampLevel"].as<int>(), 75);
    EXPECT_TRUE(doc["waterSensorFront"].as<bool>());
    EXPECT_FALSE(doc["waterSensorBack"].as<bool>());
    EXPECT_TRUE(doc["leftButton"].as<bool>());
    EXPECT_FALSE(doc["rightButton"].as<bool>());
    EXPECT_EQ(doc["batteryVoltage"].as<float>(), 14.8);
    EXPECT_EQ(doc["temperature"].as<float>(), 25.5);
    EXPECT_EQ(doc["motorSpeed"].as<int>(), 50);
    EXPECT_EQ(doc["systemStatus"].as<String>(), "running");
}

// Test status with water sensor states
TEST_F(WebApiTest, StatusWaterSensor) {
    // Test with both water sensors active
    mockSetWaterSensorFront(true);
    mockSetWaterSensorBack(true);
    
    handleApiStatus(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc["waterSensorFront"].as<bool>());
    EXPECT_TRUE(doc["waterSensorBack"].as<bool>());
    
    // Test with no water sensors active
    mockSetWaterSensorFront(false);
    mockSetWaterSensorBack(false);
    
    client = MockWiFiClient();
    handleApiStatus(&client);
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_FALSE(doc["waterSensorFront"].as<bool>());
    EXPECT_FALSE(doc["waterSensorBack"].as<bool>());
}

// Test status with button states
TEST_F(WebApiTest, StatusButton) {
    // Test with both buttons pressed
    mockSetLeftButton(true);
    mockSetRightButton(true);
    
    handleApiStatus(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc["leftButton"].as<bool>());
    EXPECT_TRUE(doc["rightButton"].as<bool>());
    
    // Test with no buttons pressed
    mockSetLeftButton(false);
    mockSetRightButton(false);
    
    client = MockWiFiClient();
    handleApiStatus(&client);
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_FALSE(doc["leftButton"].as<bool>());
    EXPECT_FALSE(doc["rightButton"].as<bool>());
}

// Test status with system states
TEST_F(WebApiTest, StatusSystem) {
    // Test with different system statuses
    mockSetSystemStatus("running");
    
    handleApiStatus(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_EQ(doc["systemStatus"].as<String>(), "running");
    
    // Test with error status
    mockSetSystemStatus("error");
    
    client = MockWiFiClient();
    handleApiStatus(&client);
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_EQ(doc["systemStatus"].as<String>(), "error");
    
    // Test with maintenance status
    mockSetSystemStatus("maintenance");
    
    client = MockWiFiClient();
    handleApiStatus(&client);
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_EQ(doc["systemStatus"].as<String>(), "maintenance");
}

// Test status with sensor values
TEST_F(WebApiTest, StatusSensors) {
    // Test with normal sensor values
    mockSetBatteryVoltage(14.8);
    mockSetTemperature(25.5);
    
    handleApiStatus(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_EQ(doc["batteryVoltage"].as<float>(), 14.8);
    EXPECT_EQ(doc["temperature"].as<float>(), 25.5);
    
    // Test with critical sensor values
    mockSetBatteryVoltage(10.0);
    mockSetTemperature(40.0);
    
    client = MockWiFiClient();
    handleApiStatus(&client);
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_EQ(doc["batteryVoltage"].as<float>(), 10.0);
    EXPECT_EQ(doc["temperature"].as<float>(), 40.0);
}

// Test info page functionality
TEST_F(WebApiTest, InfoPage) {
    // Test GET info
    handleApiInfo(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    // Check all required info fields
    EXPECT_TRUE(doc.containsKey("version"));
    EXPECT_TRUE(doc.containsKey("buildDate"));
    EXPECT_TRUE(doc.containsKey("deviceId"));
    EXPECT_TRUE(doc.containsKey("firmwareVersion"));
    EXPECT_TRUE(doc.containsKey("hardwareVersion"));
    EXPECT_TRUE(doc.containsKey("wifiSSID"));
    EXPECT_TRUE(doc.containsKey("ipAddress"));
    EXPECT_TRUE(doc.containsKey("macAddress"));
    EXPECT_TRUE(doc.containsKey("freeHeap"));
    EXPECT_TRUE(doc.containsKey("totalHeap"));
    EXPECT_TRUE(doc.containsKey("cpuFrequency"));
    EXPECT_TRUE(doc.containsKey("flashSize"));
    EXPECT_TRUE(doc.containsKey("sdkVersion"));
}

// Test info values
TEST_F(WebApiTest, InfoValues) {
    // Set mock values
    mockSetVersion("1.0.0");
    mockSetBuildDate("2024-03-20");
    mockSetDeviceId("DPV001");
    mockSetFirmwareVersion("1.0.0");
    mockSetHardwareVersion("2.0");
    mockSetWifiSSID("DPV_Network");
    mockSetIpAddress("192.168.1.100");
    mockSetMacAddress("00:11:22:33:44:55");
    mockSetFreeHeap(100000);
    mockSetTotalHeap(200000);
    mockSetCpuFrequency(240);
    mockSetFlashSize(4194304);
    mockSetSdkVersion("3.0.0");
    
    handleApiInfo(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    // Verify all info values
    EXPECT_EQ(doc["version"].as<String>(), "1.0.0");
    EXPECT_EQ(doc["buildDate"].as<String>(), "2024-03-20");
    EXPECT_EQ(doc["deviceId"].as<String>(), "DPV001");
    EXPECT_EQ(doc["firmwareVersion"].as<String>(), "1.0.0");
    EXPECT_EQ(doc["hardwareVersion"].as<String>(), "2.0");
    EXPECT_EQ(doc["wifiSSID"].as<String>(), "DPV_Network");
    EXPECT_EQ(doc["ipAddress"].as<String>(), "192.168.1.100");
    EXPECT_EQ(doc["macAddress"].as<String>(), "00:11:22:33:44:55");
    EXPECT_EQ(doc["freeHeap"].as<int>(), 100000);
    EXPECT_EQ(doc["totalHeap"].as<int>(), 200000);
    EXPECT_EQ(doc["cpuFrequency"].as<int>(), 240);
    EXPECT_EQ(doc["flashSize"].as<int>(), 4194304);
    EXPECT_EQ(doc["sdkVersion"].as<String>(), "3.0.0");
}

// Test info with different versions
TEST_F(WebApiTest, InfoVersions) {
    // Test with development version
    mockSetVersion("1.0.0-dev");
    mockSetFirmwareVersion("1.0.0-dev");
    
    handleApiInfo(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_EQ(doc["version"].as<String>(), "1.0.0-dev");
    EXPECT_EQ(doc["firmwareVersion"].as<String>(), "1.0.0-dev");
    
    // Test with release version
    mockSetVersion("1.0.0");
    mockSetFirmwareVersion("1.0.0");
    
    client = MockWiFiClient();
    handleApiInfo(&client);
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_EQ(doc["version"].as<String>(), "1.0.0");
    EXPECT_EQ(doc["firmwareVersion"].as<String>(), "1.0.0");
}

// Test info with different network states
TEST_F(WebApiTest, InfoNetwork) {
    // Test with connected network
    mockSetWifiSSID("DPV_Network");
    mockSetIpAddress("192.168.1.100");
    mockSetMacAddress("00:11:22:33:44:55");
    
    handleApiInfo(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_EQ(doc["wifiSSID"].as<String>(), "DPV_Network");
    EXPECT_EQ(doc["ipAddress"].as<String>(), "192.168.1.100");
    EXPECT_EQ(doc["macAddress"].as<String>(), "00:11:22:33:44:55");
    
    // Test with disconnected network
    mockSetWifiSSID("");
    mockSetIpAddress("0.0.0.0");
    
    client = MockWiFiClient();
    handleApiInfo(&client);
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_EQ(doc["wifiSSID"].as<String>(), "");
    EXPECT_EQ(doc["ipAddress"].as<String>(), "0.0.0.0");
}

// Test info with different hardware configurations
TEST_F(WebApiTest, InfoHardware) {
    // Test with standard configuration
    mockSetHardwareVersion("2.0");
    mockSetCpuFrequency(240);
    mockSetFlashSize(4194304);
    
    handleApiInfo(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_EQ(doc["hardwareVersion"].as<String>(), "2.0");
    EXPECT_EQ(doc["cpuFrequency"].as<int>(), 240);
    EXPECT_EQ(doc["flashSize"].as<int>(), 4194304);
    
    // Test with different configuration
    mockSetHardwareVersion("2.1");
    mockSetCpuFrequency(160);
    mockSetFlashSize(8388608);
    
    client = MockWiFiClient();
    handleApiInfo(&client);
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_EQ(doc["hardwareVersion"].as<String>(), "2.1");
    EXPECT_EQ(doc["cpuFrequency"].as<int>(), 160);
    EXPECT_EQ(doc["flashSize"].as<int>(), 8388608);
}

// Test info with memory states
TEST_F(WebApiTest, InfoMemory) {
    // Test with normal memory state
    mockSetFreeHeap(100000);
    mockSetTotalHeap(200000);
    
    handleApiInfo(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_EQ(doc["freeHeap"].as<int>(), 100000);
    EXPECT_EQ(doc["totalHeap"].as<int>(), 200000);
    
    // Test with low memory state
    mockSetFreeHeap(10000);
    mockSetTotalHeap(200000);
    
    client = MockWiFiClient();
    handleApiInfo(&client);
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_EQ(doc["freeHeap"].as<int>(), 10000);
    EXPECT_EQ(doc["totalHeap"].as<int>(), 200000);
}

// Test LED bar functionality
TEST_F(WebApiTest, LEDBar) {
    // Test setting individual LEDs
    for (int i = 0; i < 10; i++) {
        mockSetLEDBarState(i, true);
        EXPECT_TRUE(mockGetLEDBarState(i));
        mockSetLEDBarState(i, false);
        EXPECT_FALSE(mockGetLEDBarState(i));
    }
    
    // Test brightness control
    mockSetLEDBarBrightness(50);
    EXPECT_EQ(mockGetLEDBarBrightness(), 50);
    
    // Test invalid brightness
    mockSetLEDBarBrightness(150);
    EXPECT_EQ(mockGetLEDBarBrightness(), 50); // Should not change
    
    // Test pattern setting
    std::array<bool, 10> pattern = {true, false, true, false, true, false, true, false, true, false};
    mockSetLEDBarPattern(pattern);
    
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(mockGetLEDBarState(i), pattern[i]);
    }
    
    // Test clear
    mockClearLEDBar();
    for (int i = 0; i < 10; i++) {
        EXPECT_FALSE(mockGetLEDBarState(i));
    }
    
    // Test set all
    mockSetAllLEDBar(true);
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(mockGetLEDBarState(i));
    }
}

// Test LED bar patterns
TEST_F(WebApiTest, LEDBarPatterns) {
    // Test wave pattern
    for (int i = 0; i < 10; i++) {
        mockClearLEDBar();
        mockSetLEDBarState(i, true);
        if (i > 0) mockSetLEDBarState(i-1, true);
        if (i < 9) mockSetLEDBarState(i+1, true);
        
        // Verify wave pattern
        for (int j = 0; j < 10; j++) {
            if (j == i || j == i-1 || j == i+1) {
                EXPECT_TRUE(mockGetLEDBarState(j));
            } else {
                EXPECT_FALSE(mockGetLEDBarState(j));
            }
        }
    }
    
    // Test progress pattern
    for (int i = 0; i <= 10; i++) {
        mockClearLEDBar();
        for (int j = 0; j < i; j++) {
            mockSetLEDBarState(j, true);
        }
        
        // Verify progress pattern
        for (int j = 0; j < 10; j++) {
            if (j < i) {
                EXPECT_TRUE(mockGetLEDBarState(j));
            } else {
                EXPECT_FALSE(mockGetLEDBarState(j));
            }
        }
    }
}

// Test LED bar with brightness
TEST_F(WebApiTest, LEDBarBrightness) {
    // Test section 1 brightness
    mockSetLEDBarSectionBrightness(1, 50);
    EXPECT_EQ(mockGetLEDBarSectionBrightness(1), 50);
    
    // Test section 2 brightness
    mockSetLEDBarSectionBrightness(2, 75);
    EXPECT_EQ(mockGetLEDBarSectionBrightness(2), 75);
    
    // Test global brightness
    mockSetLEDBarBrightness(100);
    EXPECT_EQ(mockGetLEDBarSectionBrightness(1), 100);
    EXPECT_EQ(mockGetLEDBarSectionBrightness(2), 100);
    
    // Test invalid values
    mockSetLEDBarSectionBrightness(1, -1);
    EXPECT_EQ(mockGetLEDBarSectionBrightness(1), 100);
    mockSetLEDBarSectionBrightness(1, 101);
    EXPECT_EQ(mockGetLEDBarSectionBrightness(1), 100);
}

// Test LED bar patterns
TEST_F(WebApiTest, LEDBarPatterns) {
    // Test section 1 pattern
    std::array<bool, 10> pattern1 = {true, false, true, false, true, false, true, false, true, false};
    mockSetLEDBarSectionPattern(1, pattern1);
    auto result1 = mockGetLEDBarSectionPattern(1);
    EXPECT_EQ(result1, pattern1);
    
    // Test section 2 pattern
    std::array<bool, 10> pattern2 = {false, true, false, true, false, true, false, true, false, true};
    mockSetLEDBarSectionPattern(2, pattern2);
    auto result2 = mockGetLEDBarSectionPattern(2);
    EXPECT_EQ(result2, pattern2);
    
    // Test full pattern
    std::array<bool, 20> fullPattern;
    std::copy(pattern1.begin(), pattern1.end(), fullPattern.begin());
    std::copy(pattern2.begin(), pattern2.end(), fullPattern.begin() + 10);
    auto resultFull = mockGetLEDBarPattern();
    EXPECT_EQ(resultFull, fullPattern);
}

// Front lamp tests
TEST_F(WebApiTest, FrontLampPWM) {
    // Test PWM resolution
    mockSetFrontLampResolution(8);
    EXPECT_EQ(mockGetFrontLampResolution(), 8);
    mockSetFrontLampPWM(255);
    EXPECT_EQ(mockGetFrontLampPWM(), 255);
    
    // Test frequency
    mockSetFrontLampFrequency(10000);
    EXPECT_EQ(mockGetFrontLampFrequency(), 10000);
    
    // Test duty cycle
    mockSetFrontLampDutyCycle(50.0f);
    EXPECT_NEAR(mockGetFrontLampDutyCycle(), 50.0f, 0.1f);
    
    // Test invalid values
    mockSetFrontLampPWM(256);
    EXPECT_EQ(mockGetFrontLampPWM(), 255);
    mockSetFrontLampDutyCycle(101.0f);
    EXPECT_NEAR(mockGetFrontLampDutyCycle(), 100.0f, 0.1f);
}

// Click code tests
TEST_F(WebApiTest, ClickCodeTimeout) {
    mockSetClickCodeTimeout(1000);
    EXPECT_EQ(mockGetClickCodeTimeout(), 1000);
    
    mockSetClickCodeActive(true);
    EXPECT_TRUE(mockGetClickCodeActive());
    
    // Simulate timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    EXPECT_FALSE(mockGetClickCodeActive());
}

TEST_F(WebApiTest, ClickCodeValidation) {
    std::vector<int> code = {100, 200, 300};
    mockSetClickCode(code);
    
    // Test correct code
    EXPECT_TRUE(mockVerifyClickCode(code));
    
    // Test wrong code
    std::vector<int> wrongCode = {100, 200, 400};
    EXPECT_FALSE(mockVerifyClickCode(wrongCode));
    
    // Test wrong length
    std::vector<int> shortCode = {100, 200};
    EXPECT_FALSE(mockVerifyClickCode(shortCode));
}

TEST_F(WebApiTest, ClickCodeSequence) {
    mockSetClickCodeActive(true);
    mockClearClickCode();
    
    // Add clicks
    mockAddClick(100);
    mockAddClick(200);
    mockAddClick(300);
    
    // Verify sequence
    std::vector<int> expectedCode = {100, 200, 300};
    EXPECT_TRUE(mockVerifyClickCode(expectedCode));
    
    // Test inactive state
    mockSetClickCodeActive(false);
    mockAddClick(400);
    EXPECT_TRUE(mockVerifyClickCode(expectedCode)); // Should not change
}

// Water sensor tests
TEST_F(WebApiTest, WaterSensor_HandlesStateChanges) {
    // Test front sensor
    mockSetWaterSensorFront(true);
    EXPECT_TRUE(mockGetWaterSensorFront());
    mockSetWaterSensorFront(false);
    EXPECT_FALSE(mockGetWaterSensorFront());

    // Test back sensor
    mockSetWaterSensorBack(true);
    EXPECT_TRUE(mockGetWaterSensorBack());
    mockSetWaterSensorBack(false);
    EXPECT_FALSE(mockGetWaterSensorBack());

    // Test both sensors
    mockSetWaterSensorFront(true);
    mockSetWaterSensorBack(true);
    EXPECT_TRUE(mockGetWaterSensorFront());
    EXPECT_TRUE(mockGetWaterSensorBack());
}

TEST_F(WebApiTest, WaterSensor_BlocksMotorWhenActive) {
    // Arrange
    mockSetWaterSensorFront(true);
    client.print("{\"enabled\":true,\"speed\":50}");

    // Act
    handleApiMotor(&client);

    // Assert
    verifyErrorResponse(client.response, "Water sensor active");
    EXPECT_EQ(mockGetMotorStep(), 0);

    // Test back sensor
    mockSetWaterSensorFront(false);
    mockSetWaterSensorBack(true);
    client = MockWiFiClient();
    client.print("{\"enabled\":true,\"speed\":50}");

    handleApiMotor(&client);
    verifyErrorResponse(client.response, "Water sensor active");
    EXPECT_EQ(mockGetMotorStep(), 0);
}

TEST_F(WebApiTest, WaterSensor_AllowsLampWhenActive) {
    // Arrange
    mockSetWaterSensorFront(true);
    client.print("{\"level\":50}");

    // Act
    handleApiLamp(&client);

    // Assert
    verifySuccessResponse(client.response);
    EXPECT_EQ(mockGetLEDState(), 50);
}

// DHT22 tests
TEST_F(WebApiTest, DHT22_HandlesTemperatureRange) {
    // Test normal range
    mockSetTemperature(25.5);
    EXPECT_NEAR(mockGetTemperature(), 25.5, 0.1);

    // Test minimum temperature
    mockSetTemperature(-40.0);
    EXPECT_NEAR(mockGetTemperature(), -40.0, 0.1);

    // Test maximum temperature
    mockSetTemperature(80.0);
    EXPECT_NEAR(mockGetTemperature(), 80.0, 0.1);

    // Test invalid temperatures
    mockSetTemperature(-41.0);
    EXPECT_NEAR(mockGetTemperature(), -40.0, 0.1); // Should be clamped

    mockSetTemperature(81.0);
    EXPECT_NEAR(mockGetTemperature(), 80.0, 0.1); // Should be clamped
}

TEST_F(WebApiTest, DHT22_HandlesHumidityRange) {
    // Test normal range
    mockSetHumidity(50.0);
    EXPECT_NEAR(mockGetHumidity(), 50.0, 0.1);

    // Test minimum humidity
    mockSetHumidity(0.0);
    EXPECT_NEAR(mockGetHumidity(), 0.0, 0.1);

    // Test maximum humidity
    mockSetHumidity(100.0);
    EXPECT_NEAR(mockGetHumidity(), 100.0, 0.1);

    // Test invalid humidity
    mockSetHumidity(-1.0);
    EXPECT_NEAR(mockGetHumidity(), 0.0, 0.1); // Should be clamped

    mockSetHumidity(101.0);
    EXPECT_NEAR(mockGetHumidity(), 100.0, 0.1); // Should be clamped
}

TEST_F(WebApiTest, DHT22_HandlesReadErrors) {
    // Test read error
    mockSetDHTError(true);
    EXPECT_FALSE(mockGetDHTValid());
    EXPECT_NEAR(mockGetTemperature(), 0.0, 0.1);
    EXPECT_NEAR(mockGetHumidity(), 0.0, 0.1);

    // Test recovery
    mockSetDHTError(false);
    mockSetTemperature(25.5);
    mockSetHumidity(50.0);
    EXPECT_TRUE(mockGetDHTValid());
    EXPECT_NEAR(mockGetTemperature(), 25.5, 0.1);
    EXPECT_NEAR(mockGetHumidity(), 50.0, 0.1);
}

TEST_F(WebApiTest, DHT22_UpdatesStatusEndpoint) {
    // Arrange
    mockSetTemperature(25.5);
    mockSetHumidity(50.0);
    mockSetDHTValid(true);

    // Act
    handleApiStatus(&client);

    // Assert
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc.containsKey("temperature"));
    EXPECT_TRUE(doc.containsKey("humidity"));
    EXPECT_TRUE(doc.containsKey("dhtValid"));
    
    EXPECT_NEAR(doc["temperature"].as<float>(), 25.5, 0.1);
    EXPECT_NEAR(doc["humidity"].as<float>(), 50.0, 0.1);
    EXPECT_TRUE(doc["dhtValid"].as<bool>());
}

TEST_F(WebApiTest, DHT22_HandlesRapidChanges) {
    // Test rapid temperature changes
    const std::vector<float> temperatures = {20.0, 25.0, 30.0, 25.0, 20.0};
    for (float temp : temperatures) {
        mockSetTemperature(temp);
        EXPECT_NEAR(mockGetTemperature(), temp, 0.1);
    }

    // Test rapid humidity changes
    const std::vector<float> humidities = {40.0, 50.0, 60.0, 50.0, 40.0};
    for (float hum : humidities) {
        mockSetHumidity(hum);
        EXPECT_NEAR(mockGetHumidity(), hum, 0.1);
    }
}

TEST_F(WebApiTest, DHT22_HandlesSensorTimeout) {
    // Test sensor timeout
    mockSetDHTTimeout(true);
    EXPECT_FALSE(mockGetDHTValid());
    EXPECT_NEAR(mockGetTemperature(), 0.0, 0.1);
    EXPECT_NEAR(mockGetHumidity(), 0.0, 0.1);

    // Test recovery after timeout
    mockSetDHTTimeout(false);
    mockSetTemperature(25.5);
    mockSetHumidity(50.0);
    EXPECT_TRUE(mockGetDHTValid());
    EXPECT_NEAR(mockGetTemperature(), 25.5, 0.1);
    EXPECT_NEAR(mockGetHumidity(), 50.0, 0.1);
}

TEST_F(WebApiTest, DHT22_HandlesHeatIndex) {
    // Test normal conditions
    mockSetTemperature(25.5);
    mockSetHumidity(50.0);
    float heatIndex = mock_computeHeatIndex(mockGetTemperature(), mockGetHumidity());
    EXPECT_NEAR(heatIndex, 25.5, 0.1); // Should be close to temperature at normal conditions

    // Test hot and humid conditions
    mockSetTemperature(30.0);
    mockSetHumidity(80.0);
    heatIndex = mock_computeHeatIndex(mockGetTemperature(), mockGetHumidity());
    EXPECT_GT(heatIndex, 30.0); // Heat index should be higher than temperature

    // Test cold conditions
    mockSetTemperature(10.0);
    mockSetHumidity(50.0);
    heatIndex = mock_computeHeatIndex(mockGetTemperature(), mockGetHumidity());
    EXPECT_NEAR(heatIndex, 10.0, 0.1); // Should be close to temperature at cold conditions
}

TEST_F(WebApiTest, DHT22_HandlesDewPoint) {
    // Test normal conditions
    mockSetTemperature(25.5);
    mockSetHumidity(50.0);
    float dewPoint = mock_computeDewPoint(mockGetTemperature(), mockGetHumidity());
    EXPECT_LT(dewPoint, mockGetTemperature()); // Dew point should be lower than temperature
    EXPECT_GT(dewPoint, 0.0); // Dew point should be positive

    // Test high humidity
    mockSetTemperature(25.5);
    mockSetHumidity(90.0);
    dewPoint = mock_computeDewPoint(mockGetTemperature(), mockGetHumidity());
    EXPECT_NEAR(dewPoint, 23.8, 0.1); // Expected dew point at 25.5°C and 90% humidity

    // Test low humidity
    mockSetTemperature(25.5);
    mockSetHumidity(20.0);
    dewPoint = mock_computeDewPoint(mockGetTemperature(), mockGetHumidity());
    EXPECT_NEAR(dewPoint, 1.2, 0.1); // Expected dew point at 25.5°C and 20% humidity
}

TEST_F(WebApiTest, DHT22_HandlesComfortRatio) {
    // Test comfortable conditions
    mockSetTemperature(22.0);
    mockSetHumidity(50.0);
    ComfortState comfort;
    float ratio = mock_getComfortRatio(comfort, mockGetTemperature(), mockGetHumidity());
    EXPECT_EQ(comfort, Comfort_OK);
    EXPECT_GT(ratio, 80.0); // High comfort ratio

    // Test too hot
    mockSetTemperature(30.0);
    mockSetHumidity(50.0);
    ratio = mock_getComfortRatio(comfort, mockGetTemperature(), mockGetHumidity());
    EXPECT_EQ(comfort, Comfort_TooHot);
    EXPECT_LT(ratio, 50.0); // Low comfort ratio

    // Test too cold
    mockSetTemperature(15.0);
    mockSetHumidity(50.0);
    ratio = mock_getComfortRatio(comfort, mockGetTemperature(), mockGetHumidity());
    EXPECT_EQ(comfort, Comfort_TooCold);
    EXPECT_LT(ratio, 50.0); // Low comfort ratio
}

TEST_F(WebApiTest, DHT22_HandlesPerception) {
    // Test comfortable conditions
    mockSetTemperature(22.0);
    mockSetHumidity(50.0);
    byte perception = mock_computePerception(mockGetTemperature(), mockGetHumidity());
    EXPECT_EQ(perception, Perception_Comfy);

    // Test dry conditions
    mockSetTemperature(25.0);
    mockSetHumidity(20.0);
    perception = mock_computePerception(mockGetTemperature(), mockGetHumidity());
    EXPECT_EQ(perception, Perception_Dry);

    // Test uncomfortable conditions
    mockSetTemperature(30.0);
    mockSetHumidity(80.0);
    perception = mock_computePerception(mockGetTemperature(), mockGetHumidity());
    EXPECT_EQ(perception, Perception_VeryUnComfy);
}

TEST_F(WebApiTest, DHT22_HandlesAbsoluteHumidity) {
    // Test normal conditions
    mockSetTemperature(25.5);
    mockSetHumidity(50.0);
    float absHumidity = mock_computeAbsoluteHumidity(mockGetTemperature(), mockGetHumidity());
    EXPECT_GT(absHumidity, 0.0); // Should be positive
    EXPECT_LT(absHumidity, 30.0); // Should be reasonable value

    // Test high humidity
    mockSetTemperature(25.5);
    mockSetHumidity(90.0);
    absHumidity = mock_computeAbsoluteHumidity(mockGetTemperature(), mockGetHumidity());
    EXPECT_GT(absHumidity, 20.0); // Should be higher than normal conditions

    // Test low humidity
    mockSetTemperature(25.5);
    mockSetHumidity(20.0);
    absHumidity = mock_computeAbsoluteHumidity(mockGetTemperature(), mockGetHumidity());
    EXPECT_LT(absHumidity, 10.0); // Should be lower than normal conditions
}

// Test real-time status updates
TEST_F(WebApiTest, StatusEndpoint_RealTimeUpdates) {
    // Test initial state
    mockSetTemperature(25.0);
    mockSetHumidity(50.0);
    mockSetMotorStep(0);
    
    handleApiStatus(&client);
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_NEAR(doc["temperature"].as<float>(), 25.0, 0.1);
    EXPECT_NEAR(doc["humidity"].as<float>(), 50.0, 0.1);
    EXPECT_EQ(doc["motorSpeed"].as<int>(), 0);
    
    // Test state changes
    mockSetTemperature(26.0);
    mockSetHumidity(55.0);
    mockSetMotorStep(50);
    
    client = MockWiFiClient();
    handleApiStatus(&client);
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_NEAR(doc["temperature"].as<float>(), 26.0, 0.1);
    EXPECT_NEAR(doc["humidity"].as<float>(), 55.0, 0.1);
    EXPECT_EQ(doc["motorSpeed"].as<int>(), 50);
}

// Test system status transitions
TEST_F(WebApiTest, StatusEndpoint_SystemTransitions) {
    // Test normal operation
    mockSetSystemStatus("running");
    handleApiStatus(&client);
    verifyJsonResponse(client.response, {"systemStatus"});
    
    // Test error state
    mockSetSystemStatus("error");
    client = MockWiFiClient();
    handleApiStatus(&client);
    verifyJsonResponse(client.response, {"systemStatus", "errorMessage"});
    
    // Test maintenance state
    mockSetSystemStatus("maintenance");
    client = MockWiFiClient();
    handleApiStatus(&client);
    verifyJsonResponse(client.response, {"systemStatus", "maintenanceMessage"});
}

// Test data endpoint with large datasets
TEST_F(WebApiTest, DataEndpoint_LargeDatasets) {
    // Test with maximum allowed data points
    handleApiData(&client, "?count=1000");
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc.containsKey("data"));
    EXPECT_TRUE(doc["data"].is<JsonArray>());
    EXPECT_LE(doc["data"].size(), 1000);
    
    // Test data aggregation
    client = MockWiFiClient();
    handleApiData(&client, "?range=day&aggregate=hour");
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc.containsKey("data"));
    EXPECT_TRUE(doc["data"].is<JsonArray>());
    EXPECT_LE(doc["data"].size(), 24); // Max 24 hours
}

// Test data filtering
TEST_F(WebApiTest, DataEndpoint_Filtering) {
    // Test temperature filter
    handleApiData(&client, "?filter=temperature&min=20&max=30");
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc.containsKey("data"));
    JsonArray data = doc["data"];
    for (JsonVariant value : data) {
        float temp = value["temperature"].as<float>();
        EXPECT_GE(temp, 20.0);
        EXPECT_LE(temp, 30.0);
    }
    
    // Test motor speed filter
    client = MockWiFiClient();
    handleApiData(&client, "?filter=motorSpeed&min=50&max=100");
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    data = doc["data"];
    for (JsonVariant value : data) {
        int speed = value["motorSpeed"].as<int>();
        EXPECT_GE(speed, 50);
        EXPECT_LE(speed, 100);
    }
}

// Test session transitions
TEST_F(WebApiTest, SessionEndpoint_Transitions) {
    // Test session start
    String startBody = "{\"action\":\"start\",\"name\":\"test_session\"}";
    client.print(startBody);
    handleApiSession(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc["success"].as<bool>());
    EXPECT_TRUE(doc.containsKey("sessionId"));
    
    // Test session pause
    String pauseBody = "{\"action\":\"pause\"}";
    client = MockWiFiClient();
    client.print(pauseBody);
    handleApiSession(&client);
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc["success"].as<bool>());
    EXPECT_TRUE(doc.containsKey("status"));
    EXPECT_EQ(doc["status"].as<String>(), "paused");
    
    // Test session resume
    String resumeBody = "{\"action\":\"resume\"}";
    client = MockWiFiClient();
    client.print(resumeBody);
    handleApiSession(&client);
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc["success"].as<bool>());
    EXPECT_TRUE(doc.containsKey("status"));
    EXPECT_EQ(doc["status"].as<String>(), "running");
}

// Test session validation
TEST_F(WebApiTest, SessionEndpoint_Validation) {
    // Test invalid session name
    String invalidNameBody = "{\"action\":\"start\",\"name\":\"invalid/name\"}";
    client.print(invalidNameBody);
    handleApiSession(&client);
    
    verifyErrorResponse(client.response, "Invalid session name");
    
    // Test duplicate session
    String duplicateBody = "{\"action\":\"start\",\"name\":\"existing_session\"}";
    client = MockWiFiClient();
    client.print(duplicateBody);
    handleApiSession(&client);
    
    verifyErrorResponse(client.response, "Session already exists");
    
    // Test invalid action
    String invalidActionBody = "{\"action\":\"invalid\"}";
    client = MockWiFiClient();
    client.print(invalidActionBody);
    handleApiSession(&client);
    
    verifyErrorResponse(client.response, "Invalid action");
}

// Test session metadata
TEST_F(WebApiTest, SessionEndpoint_Metadata) {
    // Test session creation with metadata
    String metadataBody = R"({
        "action": "start",
        "name": "test_session",
        "metadata": {
            "description": "Test session",
            "tags": ["test", "automated"],
            "parameters": {
                "motorSpeed": 75,
                "temperature": 25.5
            }
        }
    })";
    client.print(metadataBody);
    handleApiSession(&client);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc["success"].as<bool>());
    EXPECT_TRUE(doc.containsKey("sessionId"));
    EXPECT_TRUE(doc.containsKey("metadata"));
    
    // Test metadata retrieval
    String sessionId = doc["sessionId"].as<String>();
    client = MockWiFiClient();
    handleApiSessionData(&client, sessionId.c_str());
    
    doc.clear();
    deserializeJson(doc, client.response);
    
    EXPECT_TRUE(doc.containsKey("metadata"));
    JsonObject metadata = doc["metadata"];
    EXPECT_EQ(metadata["description"].as<String>(), "Test session");
    EXPECT_TRUE(metadata["tags"].is<JsonArray>());
    EXPECT_EQ(metadata["parameters"]["motorSpeed"].as<int>(), 75);
    EXPECT_NEAR(metadata["parameters"]["temperature"].as<float>(), 25.5, 0.1);
} 