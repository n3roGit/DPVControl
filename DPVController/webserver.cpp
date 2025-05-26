#include "webserver.h"
#include "log.h"
#include "data_upload.h"
#include "datalog.h"  // Einbinden des Datalogger-Headers
#include "constants.h" // Für PIN-Definitionen
#include "beep.h" // For beeper settings
#include "settings.h" // For DPV settings system
#include "motor.h" // For motor control functions
#include "ledLamp.h" // For lamp control functions
#include "ledBar.h" // For LED bar functions
#include <LittleFS.h> // Add missing LittleFS include
#include <ArduinoJson.h> // For JSON parsing

// External variables
extern int LED_State; // From ledLamp.cpp
extern int currentMotorStep; // From motor.cpp
extern MotorState motorState; // From motor.cpp
extern unsigned long lastActionTime; // From main.cpp

// Task handle for the webserver task
TaskHandle_t webserverTaskHandle = NULL;

// WiFi Credentials
const char* ssid = "DPVControl";
const char* password = "DPVControl";

// DNS Server for captive portal
const byte DNS_PORT = 53;
IPAddress apIP(4, 3, 2, 1);
DNSServer dnsServer;
WiFiServer server(80);

// Flag to check if SPIFFS is mounted
bool spiffsInitialized = false;

/**
 * Generate JSON list of all available sessions sorted with newest first
 */
String generateSessionListJson() {
    int count;
    String* sessions = listSessionFiles(&count);
    String currentSession = getCurrentSessionFile();
    // Extract filename from full path
    if (currentSession.startsWith("/datalog/")) {
        currentSession = currentSession.substring(9); // Remove "/datalog/"
    }
    
    // Sort sessions by filename (newest first for 4-digit numbering)
    // Bubble sort for simplicity
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (sessions[j] < sessions[j + 1]) { // Reverse order for newest first
                String temp = sessions[j];
                sessions[j] = sessions[j + 1];
                sessions[j + 1] = temp;
            }
        }
    }
    
    String json = "[";
    for (int i = 0; i < count; i++) {
        if (i > 0) json += ",";
        
        // Create an object with filename and current flag
        json += "{";
        json += "\"filename\":\"" + sessions[i] + "\",";
        json += "\"isCurrent\":" + String(sessions[i] == currentSession ? "true" : "false");
        json += "}";
    }
    json += "]";
    
    return json;
}

/**
 * Generate JSON data for a specific session
 */
String generateSessionDataJson(String sessionFile) {
    // Ensure we have the full path
    String fullPath = sessionFile;
    if (!sessionFile.startsWith("/datalog/")) {
        fullPath = "/datalog/" + sessionFile;
    }
    
    if (!LittleFS.exists(fullPath)) {
        return "[]";
    }
    
    File file = LittleFS.open(fullPath, "r");
    if (!file) {
        return "[]";
    }
    
    String json = "[";
    LogdataRow dataPoint;
    bool firstPoint = true;
    
    while (file.available()) {
        size_t bytesRead = file.read((uint8_t*)&dataPoint, sizeof(LogdataRow));
        if (bytesRead != sizeof(LogdataRow)) break;
        
        if (!firstPoint) json += ",";
        firstPoint = false;
        
        json += "{";
        json += "\"timestamp\":" + String(dataPoint.timestamp) + ",";
        json += "\"tempMotor\":" + String(dataPoint.tempMotor) + ",";
        json += "\"tempMosfet\":" + String(dataPoint.tempMosfet) + ",";
        json += "\"batteryVoltage\":" + String(dataPoint.batteryVoltage) + ",";
        json += "\"current\":" + String(dataPoint.current) + ",";
        json += "\"avgMotorCurrent\":" + String(dataPoint.avgMotorCurrent) + ",";
        json += "\"rpm\":" + String(dataPoint.rpm) + ",";
        json += "\"dutyCycle\":" + String(dataPoint.dutyCycle) + ",";
        json += "\"temperature\":" + String(dataPoint.temperature) + ",";
        json += "\"humidity\":" + String(dataPoint.humidity) + ",";
        json += "\"batteryLevel\":" + String(dataPoint.batteryLevel) + ",";
        json += "\"leakSensorState\":" + String(dataPoint.leakSensorState) + ",";
        json += "\"ledState\":" + String(dataPoint.ledState) + ",";
        json += "\"totalUptime\":" + String(dataPoint.totalUptime);
        json += "}";
    }
    
    json += "]";
    file.close();
    
    return json;
}

// Simple Hello World HTML
const char* helloWorldHTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>DPVControl</title>
    <style>
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            margin: 0;
            padding: 0;
            background-color: #1a1a1a;
            color: #e0e0e0;
        }
        h1, h2 {
            color: #4fc3f7;
        }
        .container {
            max-width: 800px;
            margin: 20px auto;
            padding: 20px;
            background-color: #2d2d2d;
            border-radius: 8px;
            box-shadow: 0 4px 12px rgba(0, 0, 0, 0.3);
            border: 1px solid #404040;
        }
        .section {
            margin-bottom: 20px;
            padding: 15px;
            border-bottom: 1px solid #404040;
        }
        table {
            width: 100%;
            border-collapse: collapse;
        }
        th, td {
            padding: 8px;
            text-align: left;
            border-bottom: 1px solid #404040;
            color: #e0e0e0;
        }
        .button {
            background-color: #2196f3;
            color: white;
            border: none;
            padding: 10px 15px;
            border-radius: 6px;
            cursor: pointer;
            margin: 5px;
            transition: background-color 0.3s ease;
            font-weight: 500;
        }
        .button:hover {
            background-color: #1976d2;
        }
        .nav-tab {
            padding: 10px 20px;
            background-color: #3d3d3d;
            border: none;
            cursor: pointer;
            transition: background-color 0.3s ease;
            margin-right: 5px;
            color: #e0e0e0;
            border-radius: 6px 6px 0 0;
        }
        .nav-tab:hover {
            background-color: #4a4a4a;
        }
        .nav-tab.active {
            background-color: #2196f3;
            color: white;
        }
        .tab-content {
            display: none;
        }
        .tab-content.active {
            display: block;
        }
        .status-value {
            font-weight: bold;
            color: #4fc3f7;
        }
        .settings-group {
            margin-bottom: 25px;
            padding: 15px;
            background-color: #3a3a3a;
            border-radius: 8px;
            border: 1px solid #505050;
        }
        .settings-group h3 {
            margin-top: 0;
            color: #4fc3f7;
            border-bottom: 2px solid #2196f3;
            padding-bottom: 5px;
        }
        .settings-table {
            width: 100%;
            margin-top: 10px;
        }
        .settings-table td:first-child {
            width: 40%;
            font-weight: bold;
            color: #b0b0b0;
        }
        .settings-table input, .settings-table select {
            width: 100%;
            padding: 6px;
            border: 1px solid #555;
            border-radius: 4px;
            background-color: #4a4a4a;
            color: #e0e0e0;
        }
        .settings-table input:focus, .settings-table select:focus {
            outline: none;
            border-color: #2196f3;
            box-shadow: 0 0 5px rgba(33, 150, 243, 0.3);
        }
        .settings-table input[type="checkbox"] {
            width: auto;
            transform: scale(1.2);
        }
        /* Range slider styling */
        input[type="range"] {
            -webkit-appearance: none;
            appearance: none;
            background: transparent;
            cursor: pointer;
        }
        input[type="range"]::-webkit-slider-track {
            background: #404040;
            height: 6px;
            border-radius: 3px;
        }
        input[type="range"]::-webkit-slider-thumb {
            -webkit-appearance: none;
            appearance: none;
            background: #2196f3;
            height: 20px;
            width: 20px;
            border-radius: 50%;
            cursor: pointer;
        }
        input[type="range"]::-moz-range-track {
            background: #404040;
            height: 6px;
            border-radius: 3px;
            border: none;
        }
        input[type="range"]::-moz-range-thumb {
            background: #2196f3;
            height: 20px;
            width: 20px;
            border-radius: 50%;
            cursor: pointer;
            border: none;
        }
    </style>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
</head>
<body>
    <div class="container">
        <h1>DPVControl Web Interface</h1>
        
        <div class="section">
            <div class="tab-navigation">
                <button class="nav-tab active" onclick="showTab('status')">Status</button>
                <button class="nav-tab" onclick="showTab('data')">Data</button>
                <button class="nav-tab" onclick="showTab('charts')">Charts</button>
                <button class="nav-tab" onclick="showTab('remote')">Remote Control</button>
                <button class="nav-tab" onclick="showTab('settings')">Settings</button>
                <button class="nav-tab" onclick="showTab('info')">Info</button>
            </div>
        </div>
        
        <div id="status-tab" class="tab-content active">
            <div class="section">
                <h2>System Status</h2>
                <table>
                    <tr>
                        <td>Uptime:</td>
                        <td class="status-value" id="uptime">Loading...</td>
                    </tr>
                    <tr>
                        <td>Total Uptime:</td>
                        <td class="status-value" id="totalUptime">Loading...</td>
                    </tr>
                    <tr>
                        <td>Battery Voltage:</td>
                        <td class="status-value" id="battery">Loading...</td>
                    </tr>
                    <tr>
                        <td>Motor Temperature:</td>
                        <td class="status-value" id="motorTemp">Loading...</td>
                    </tr>
                    <tr>
                        <td>MOSFET Temperature:</td>
                        <td class="status-value" id="mosfetTemp">Loading...</td>
                    </tr>
                    <tr>
                        <td>Ambient Temperature:</td>
                        <td class="status-value" id="temperature">Loading...</td>
                    </tr>
                    <tr>
                        <td>Humidity:</td>
                        <td class="status-value" id="humidity">Loading...</td>
                    </tr>
                    <tr>
                        <td>Input Current:</td>
                        <td class="status-value" id="current">Loading...</td>
                    </tr>
                    <tr>
                        <td>Motor Current:</td>
                        <td class="status-value" id="motorCurrent">Loading...</td>
                    </tr>
                    <tr>
                        <td>eRPM:</td>
                        <td class="status-value" id="rpm">Loading...</td>
                    </tr>
                    <tr>
                        <td>Duty Cycle:</td>
                        <td class="status-value" id="dutyCycle">Loading...</td>
                    </tr>
                    <tr>
                        <td>Battery Level:</td>
                        <td class="status-value" id="batteryLevel">Loading...</td>
                    </tr>
                    <tr>
                        <td>Water Sensor Front:</td>
                        <td class="status-value" id="waterSensorFront">Loading...</td>
                    </tr>
                    <tr>
                        <td>Water Sensor Back:</td>
                        <td class="status-value" id="waterSensorBack">Loading...</td>
                    </tr>
                    <tr>
                        <td>Left Button:</td>
                        <td class="status-value" id="leftButton">Loading...</td>
                    </tr>
                    <tr>
                        <td>Right Button:</td>
                        <td class="status-value" id="rightButton">Loading...</td>
                    </tr>
                    <tr>
                        <td>Lamp Level:</td>
                        <td class="status-value" id="lampLevel">Loading...</td>
                    </tr>
                    <tr>
                        <td>Beeper:</td>
                        <td class="status-value" id="beeperStatus">Loading...</td>
                    </tr>
                </table>
            </div>
        </div>
        
        <div id="data-tab" class="tab-content">
            <div class="section">
                <h2>Recent Data Points</h2>
                <div style="margin-bottom: 15px;">
                    <label for="dataPoints">Number of data points to show:</label>
                    <input type="number" id="dataPoints" min="10" max="100" value="20" style="width: 80px; margin-left: 10px;">
                    <button class="button" onclick="loadRecentData()" style="margin-left: 10px;">Refresh Data</button>
                </div>
                <p>Data Points Available: <span id="dataPointCount">Loading...</span></p>
                <div id="dataDisplay" style="margin-top: 20px;">
                    <p>Click "Refresh Data" to load recent measurements...</p>
                </div>
            </div>
        </div>
        
        <div id="charts-tab" class="tab-content">
            <div class="section">
                <h2>Data Visualization</h2>
                
                <!-- Chart Controls -->
                <div style="margin-bottom: 20px; padding: 15px; background-color: #f5f5f5; border-radius: 5px;">
                    <div style="display: flex; flex-wrap: wrap; gap: 15px; align-items: center;">
                        <div>
                            <label for="sessionSelect">Session:</label>
                            <select id="sessionSelect" onchange="updateSessionFilter()">
                                <!-- Sessions will be populated dynamically -->
                            </select>
                        </div>
                        
                        <div style="display: flex; align-items: center; gap: 10px;">
                            <span>Time Range: <strong>5 Minutes</strong></span>
                            <span style="color: #666; font-size: 12px;">(Fixed)</span>
                        </div>
                        
                        <div id="timeSliderContainer" style="flex: 1; min-width: 200px;">
                            <label for="timeSlider">Time Window Position:</label>
                            <input type="range" id="timeSlider" min="0" max="100" value="100" 
                                   style="width: 100%;" onchange="updateTimeWindow()">
                            <div style="display: flex; justify-content: space-between; font-size: 12px; color: #666;">
                                <span id="sliderStart">Oldest</span>
                                <span id="sliderEnd">Newest</span>
                            </div>
                        </div>
                        
                        <div style="display: flex; align-items: center; gap: 10px;">
                            <span>Update: <strong>10s</strong></span>
                            <span style="color: #666; font-size: 12px;">(Fixed)</span>
                        </div>
                        
                        <button class="button" onclick="refreshChart()">Refresh</button>
                        
                        <button class="button" onclick="deleteAllSessions()" style="background-color: #f44336; margin-left: 20px;">Delete All Sessions</button>
                    </div>
                </div>
                
                <!-- Chart Container -->
                <div style="height: 400px; width: 100%; position: relative;">
                    <canvas id="combinedChart"></canvas>
                </div>
                
                <!-- Download Buttons -->
                <div style="margin-top: 15px; text-align: center;">
                    <button class="button" onclick="exportCurrentViewAsCSV()" style="background-color: #27ae60; margin-right: 10px;">
                        Export Current View as CSV
                    </button>
                    <button class="button" onclick="exportFullTripLogAsCSV()" style="background-color: #e67e22;">
                        Download Full Trip Log as CSV
                    </button>
                </div>
                
                <p style="margin-top: 10px; font-size: 12px; color: #666;">
                    Note: Different parameters use different scales. Red vertical lines indicate system restarts.
                </p>
            </div>
        </div>
        
        <div id="remote-tab" class="tab-content">
            <div class="section">
                <h2>Remote Control</h2>
                <p style="text-align: center; color: #ff9800; margin-bottom: 25px;">
                    ⚠️ <strong>Warning:</strong> Use remote control carefully. Ensure propeller area is clear before starting motor.
                </p>
                
                <!-- Motor Control -->
                <div class="settings-group">
                    <h3>Motor Control</h3>
                    <div style="text-align: center; margin-bottom: 20px;">
                        <button id="motorToggle" class="button" onclick="toggleMotor()" style="background-color: #4caf50; font-size: 18px; padding: 15px 30px;">
                            START MOTOR
                        </button>
                    </div>
                    
                    <div style="margin-bottom: 20px;">
                        <label for="motorSpeedSlider" style="display: block; margin-bottom: 10px; font-weight: bold;">
                            Motor Speed: <span id="motorSpeedValue">0</span>%
                        </label>
                        <input type="range" id="motorSpeedSlider" min="0" max="100" value="0" 
                               style="width: 100%; height: 6px;" 
                               oninput="updateMotorSpeed(this.value)" onchange="setMotorSpeed(this.value)">
                        <div style="display: flex; justify-content: space-between; font-size: 12px; color: #b0b0b0; margin-top: 5px;">
                            <span>0%</span>
                            <span>25%</span>
                            <span>50%</span>
                            <span>75%</span>
                            <span>100%</span>
                        </div>
                    </div>
                    
                    <div style="text-align: center;">
                        <button class="button" onclick="emergencyStop()" style="background-color: #f44336; font-size: 16px; padding: 12px 25px;">
                            🛑 EMERGENCY STOP
                        </button>
                    </div>
                </div>
                
                <!-- Lamp Control -->
                <div class="settings-group">
                    <h3>Front Lamp Control</h3>
                    <div style="margin-bottom: 20px;">
                        <label for="lampLevelSlider" style="display: block; margin-bottom: 10px; font-weight: bold;">
                            Lamp Level: <span id="lampLevelValue">0</span> (OFF)
                        </label>
                        <input type="range" id="lampLevelSlider" min="0" max="4" value="0" 
                               style="width: 100%; height: 6px;" 
                               oninput="updateLampLevel(this.value)" onchange="setLampLevel(this.value)">
                        <div style="display: flex; justify-content: space-between; font-size: 12px; color: #b0b0b0; margin-top: 5px;">
                            <span>OFF</span>
                            <span>Level 1</span>
                            <span>Level 2</span>
                            <span>Level 3</span>
                            <span>MAX</span>
                        </div>
                    </div>
                    
                    <div style="text-align: center;">
                        <button class="button" onclick="setLampLevel(0)" style="background-color: #666;">
                            💡 Turn OFF
                        </button>
                    </div>
                </div>
                
                <!-- Status Display -->
                <div class="settings-group">
                    <h3>Control Status</h3>
                    <table style="width: 100%;">
                        <tr>
                            <td><strong>Motor Status:</strong></td>
                            <td id="remoteMotorStatus" style="color: #4fc3f7;">STOPPED</td>
                        </tr>
                        <tr>
                            <td><strong>Current Speed:</strong></td>
                            <td id="remoteMotorSpeed" style="color: #4fc3f7;">0%</td>
                        </tr>
                        <tr>
                            <td><strong>Lamp Status:</strong></td>
                            <td id="remoteLampStatus" style="color: #4fc3f7;">OFF</td>
                        </tr>
                        <tr>
                            <td><strong>Last Command:</strong></td>
                            <td id="remoteLastCommand" style="color: #4fc3f7;">-</td>
                        </tr>
                    </table>
                </div>
                
                <div id="remoteControlStatus" style="margin-top: 20px; text-align: center; color: #4fc3f7;"></div>
            </div>
        </div>
        
        <div id="settings-tab" class="tab-content">
            <div class="section">
                <h2>DPV Settings</h2>
                
                <!-- Settings Controls -->
                <div style="margin-bottom: 20px; text-align: center;">
                    <button class="button" onclick="loadDPVSettings()" style="background-color: #2196f3;">Reload</button>
                    <button class="button" onclick="saveDPVSettings()" style="background-color: #4caf50;">Save Settings</button>
                    <button class="button" onclick="restoreDefaultSettings()" style="background-color: #f44336;">Restore Defaults</button>
                    <br>
                    <button class="button" onclick="exportSettings()" style="background-color: #ff9800; margin-top: 10px;">Export Settings</button>
                    <button class="button" onclick="importSettings()" style="background-color: #9c27b0; margin-top: 10px;">Import Settings</button>
                    <input type="file" id="settingsFileInput" accept=".json" style="display: none;" onchange="handleSettingsFile(event)">
                </div>
                
                <!-- Settings Form -->
                <form id="settingsForm">
                    <!-- Motor and Speed Settings -->
                    <div class="settings-group">
                        <h3>Motor & Speed Settings</h3>
                        <table class="settings-table">
                            <tr>
                                <td><label for="speedSteps">Speed Steps:</label></td>
                                <td><input type="number" id="speedSteps" min="1" max="20" value="10"></td>
                            </tr>
                            <tr>
                                <td><label for="standbyDelaySeconds">Standby Delay (seconds):</label></td>
                                <td><input type="number" id="standbyDelaySeconds" min="10" max="600" value="60"></td>
                            </tr>
                            <tr>
                                <td><label for="batteryPowerMax">Battery Power Max (A):</label></td>
                                <td><input type="number" id="batteryPowerMax" min="10" max="100" value="40"></td>
                            </tr>
                            <tr>
                                <td><label for="minSpeedPercent">Min Speed Percent:</label></td>
                                <td><input type="number" id="minSpeedPercent" min="0.1" max="1.0" step="0.01" value="0.38"></td>
                            </tr>
                            <tr>
                                <td><label for="maxSpeedRpm">Max Speed RPM:</label></td>
                                <td><input type="number" id="maxSpeedRpm" min="1000" max="50000" value="15800"></td>
                            </tr>
                            <tr>
                                <td><label for="speedUpTimeMs">Speed Up Time (ms):</label></td>
                                <td><input type="number" id="speedUpTimeMs" min="100" max="10000" value="3000"></td>
                            </tr>
                            <tr>
                                <td><label for="speedDownTimeMs">Speed Down Time (ms):</label></td>
                                <td><input type="number" id="speedDownTimeMs" min="50" max="5000" value="500"></td>
                            </tr>
                            <tr>
                                <td><label for="maxTimeOverloadedMs">Max Overload Time (ms):</label></td>
                                <td><input type="number" id="maxTimeOverloadedMs" min="1000" max="30000" value="5000"></td>
                            </tr>
                        </table>
                    </div>
                    
                    <!-- Jam Detection -->
                    <div class="settings-group">
                        <h3>Jam Detection</h3>
                        <table class="settings-table">
                            <tr>
                                <td><label for="jamMin">Jam Min:</label></td>
                                <td><input type="number" id="jamMin" min="0.05" max="0.5" step="0.01" value="0.2"></td>
                            </tr>
                            <tr>
                                <td><label for="jamDetectionThreshold">Jam Detection Threshold:</label></td>
                                <td><input type="number" id="jamDetectionThreshold" min="0.1" max="1.0" step="0.01" value="0.5"></td>
                            </tr>
                        </table>
                    </div>
                    
                    <!-- Battery Settings -->
                    <div class="settings-group">
                        <h3>Battery Settings</h3>
                        <table class="settings-table">
                            <tr>
                                <td><label for="cellsInSeries">Cells in Series:</label></td>
                                <td><input type="number" id="cellsInSeries" min="1" max="20" value="13"></td>
                            </tr>
                        </table>
                    </div>
                    
                    <!-- LED Bar Settings -->
                    <div class="settings-group">
                        <h3>LED Bar Settings</h3>
                        <table class="settings-table">
                            <tr>
                                <td><label for="ledBarNum">LED Bar Number:</label></td>
                                <td><input type="number" id="ledBarNum" min="1" max="50" value="10"></td>
                            </tr>
                            <tr>
                                <td><label for="ledBarBrightness">LED Bar Brightness:</label></td>
                                <td><input type="number" id="ledBarBrightness" min="1" max="255" value="15"></td>
                            </tr>
                            <tr>
                                <td><label for="ledBarBrightnessSecond">LED Bar Brightness Second:</label></td>
                                <td><input type="number" id="ledBarBrightnessSecond" min="1" max="255" value="3"></td>
                            </tr>
                            <tr>
                                <td><label for="ledFrequency">LED Frequency:</label></td>
                                <td><input type="number" id="ledFrequency" min="100" max="10000" value="960"></td>
                            </tr>
                        </table>
                    </div>
                    
                    <!-- Lamp Settings -->
                    <div class="settings-group">
                        <h3>Front Lamp Settings</h3>
                        <table class="settings-table">
                            <tr>
                                <td><label for="lampMaxLevels">Number of Lamp Levels:</label></td>
                                <td><input type="number" id="lampMaxLevels" min="2" max="10" value="5" onchange="updateLampBrightnessInputs()"></td>
                            </tr>
                        </table>
                        <div id="lampBrightnessContainer">
                            <!-- Lamp brightness inputs will be generated dynamically -->
                        </div>
                    </div>
                    
                    <!-- WiFi Settings -->
                    <div class="settings-group">
                        <h3>WiFi Settings</h3>
                        <table class="settings-table">
                            <tr>
                                <td><label for="wifiSSID">WiFi SSID:</label></td>
                                <td><input type="text" id="wifiSSID" maxlength="31" value="DPVControl"></td>
                            </tr>
                            <tr>
                                <td><label for="wifiPassword">WiFi Password:</label></td>
                                <td><input type="password" id="wifiPassword" maxlength="31" value="DPVControl"></td>
                            </tr>
                        </table>
                    </div>
                    
                    <!-- System Settings -->
                    <div class="settings-group">
                        <h3>System Settings</h3>
                        <table class="settings-table">
                            <tr>
                                <td><label for="beeperEnabled">Beeper Enabled:</label></td>
                                <td><input type="checkbox" id="beeperEnabled" checked></td>
                            </tr>
                            <tr>
                                <td><label for="debugLoggingEnabled">Debug Logging Enabled:</label></td>
                                <td><input type="checkbox" id="debugLoggingEnabled"></td>
                            </tr>
                            <tr>
                                <td><label for="standbyBlinkStartMinutes">Standby Blink Start (min):</label></td>
                                <td><input type="number" id="standbyBlinkStartMinutes" min="1" max="60" value="15"></td>
                            </tr>
                            <tr>
                                <td><label for="standbyBlinkDurationSeconds">Standby Blink Duration (s):</label></td>
                                <td><input type="number" id="standbyBlinkDurationSeconds" min="1" max="60" value="10"></td>
                            </tr>
                        </table>
                    </div>
                </form>
                
                <div id="settingsStatus" style="margin-top: 20px; text-align: center; color: #2c3e50;"></div>
            </div>
        </div>
        
        <div id="info-tab" class="tab-content">
            <div class="section">
                <h2>DPVControl Information</h2>
                
                <!-- Logo and Header -->
                <div style="text-align: center; margin-bottom: 30px;">
                    <img src="https://github.com/BubTec/DPVControl/blob/production_repo/images/logo.jpg?raw=true" 
                         alt="DPVControl Logo" 
                         style="max-width: 200px; max-height: 100px; margin-bottom: 15px;" 
                         onerror="this.style.display='none'">
                    <h3 style="margin: 0; color: #4fc3f7;">DPV Control System</h3>
                    <p style="margin: 5px 0; color: #b0b0b0;">Diver Propulsion Vehicle Control Unit</p>
                </div>
                
                <!-- Version Information -->
                <div class="settings-group">
                    <h3>System Information</h3>
                    <table class="settings-table">
                        <tr>
                            <td>Current Version:</td>
                            <td id="currentVersion" style="color: #4fc3f7; font-weight: bold;">Loading...</td>
                        </tr>
                        <tr>
                            <td>System Uptime:</td>
                            <td id="systemUptime" style="color: #4fc3f7;">Loading...</td>
                        </tr>
                        <tr>
                            <td>Total Runtime:</td>
                            <td id="totalRuntime" style="color: #4fc3f7;">Loading...</td>
                        </tr>
                    </table>
                </div>
                
                <!-- Button Control Guide -->
                <div class="settings-group">
                    <h3>Button Control Guide</h3>
                    <p style="color: #b0b0b0; margin-bottom: 15px;">Control your DPV using the left and right buttons with these commands:</p>
                    
                    <table style="width: 100%; border-collapse: collapse; background-color: #3a3a3a;">
                        <thead>
                            <tr style="background-color: #2196f3; color: white;">
                                <th style="border: 1px solid #505050; padding: 10px; text-align: center;">Left Button</th>
                                <th style="border: 1px solid #505050; padding: 10px; text-align: center;">Right Button</th>
                                <th style="border: 1px solid #505050; padding: 10px; text-align: center;">Function</th>
                            </tr>
                        </thead>
                        <tbody>
                            <!-- Motor Control -->
                            <tr>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center; background-color: #4a4a4a;"><strong>Hold</strong></td>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center; background-color: #4a4a4a;"><strong>Hold</strong></td>
                                <td style="border: 1px solid #505050; padding: 8px; color: #4fc3f7;"><strong>Turn motor ON</strong></td>
                            </tr>
                            <tr>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center;"><strong>Hold</strong></td>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center;">-</td>
                                <td style="border: 1px solid #505050; padding: 8px; color: #4fc3f7;">Turn motor ON</td>
                            </tr>
                            <tr>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center;">-</td>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center;"><strong>Hold</strong></td>
                                <td style="border: 1px solid #505050; padding: 8px; color: #4fc3f7;">Turn motor ON</td>
                            </tr>
                            
                            <!-- Cruise Control -->
                            <tr>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center; background-color: #4a4a4a;">1 Click</td>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center; background-color: #4a4a4a;">1 Click</td>
                                <td style="border: 1px solid #505050; padding: 8px; color: #4fc3f7;"><strong>Cruise Control</strong> (within 150ms)</td>
                            </tr>
                            
                            <!-- Speed Control -->
                            <tr>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center; background-color: #4a4a4a;">2 Clicks</td>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center; background-color: #4a4a4a;">-</td>
                                <td style="border: 1px solid #505050; padding: 8px; color: #4fc3f7;">Speed Down</td>
                            </tr>
                            <tr>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center;">-</td>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center;">2 Clicks</td>
                                <td style="border: 1px solid #505050; padding: 8px; color: #4fc3f7;">Speed Up</td>
                            </tr>
                            <tr>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center; background-color: #4a4a4a;">2 Clicks</td>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center; background-color: #4a4a4a;">2 Clicks</td>
                                <td style="border: 1px solid #505050; padding: 8px; color: #4fc3f7;">Wake up from Standby</td>
                            </tr>
                            
                            <!-- Turbo Mode -->
                            <tr>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center; background-color: #4a4a4a;"><strong>2 Clicks + Hold</strong></td>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center; background-color: #4a4a4a;"><strong>2 Clicks + Hold</strong></td>
                                <td style="border: 1px solid #505050; padding: 8px; color: #4fc3f7;"><strong>Turbo Mode</strong> (Maximum Power)</td>
                            </tr>
                            
                            <!-- Lighting -->
                            <tr>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center; background-color: #4a4a4a;">3 Clicks</td>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center; background-color: #4a4a4a;">-</td>
                                <td style="border: 1px solid #505050; padding: 8px; color: #4fc3f7;">Short Light Flash</td>
                            </tr>
                            <tr>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center;">-</td>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center;">3 Clicks</td>
                                <td style="border: 1px solid #505050; padding: 8px; color: #4fc3f7;">Toggle Light Level (0→1→2→3→4→0)</td>
                            </tr>
                            <tr>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center; background-color: #4a4a4a;">3 Clicks</td>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center; background-color: #4a4a4a;">3 Clicks</td>
                                <td style="border: 1px solid #505050; padding: 8px; color: #ff9800;">PowerBank ON/OFF (Not implemented)</td>
                            </tr>
                            
                            <!-- Battery Information -->
                            <tr>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center; background-color: #4a4a4a;">4 Clicks</td>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center; background-color: #4a4a4a;">-</td>
                                <td style="border: 1px solid #505050; padding: 8px; color: #4fc3f7;">Battery Level Beep Output</td>
                            </tr>
                        </tbody>
                    </table>
                </div>
                
                <!-- Beep Codes -->
                <div class="settings-group">
                    <h3>Beep Codes Reference</h3>
                    <p style="color: #b0b0b0; margin-bottom: 15px;">Understanding system status through audio signals:</p>
                    <p style="color: #b0b0b0; font-size: 14px; margin-bottom: 15px;">
                        <strong>Legend:</strong> 1 = short beep, 2 = long beep
                    </p>
                    
                    <table style="width: 100%; border-collapse: collapse; background-color: #3a3a3a;">
                        <thead>
                            <tr style="background-color: #2196f3; color: white;">
                                <th style="border: 1px solid #505050; padding: 10px; text-align: center;">Beep Pattern</th>
                                <th style="border: 1px solid #505050; padding: 10px; text-align: center;">Meaning</th>
                            </tr>
                        </thead>
                        <tbody>
                            <tr>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center; background-color: #4a4a4a;"><strong>12121212</strong></td>
                                <td style="border: 1px solid #505050; padding: 8px; color: #f44336;"><strong>⚠️ LEAK WARNING</strong></td>
                            </tr>
                            <tr>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center;">1</td>
                                <td style="border: 1px solid #505050; padding: 8px; color: #4fc3f7;">Still in standby / Boot confirmation</td>
                            </tr>
                            <tr>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center; background-color: #4a4a4a;">11</td>
                                <td style="border: 1px solid #505050; padding: 8px; color: #4fc3f7;">Going to standby / Wake up from standby</td>
                            </tr>
                            <tr>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center;">111</td>
                                <td style="border: 1px solid #505050; padding: 8px; color: #ff9800;">No speed up (overloaded)</td>
                            </tr>
                            <tr>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center; background-color: #4a4a4a;">111222111</td>
                                <td style="border: 1px solid #505050; padding: 8px; color: #f44336;">SOS Signal (Long standby)</td>
                            </tr>
                            <tr>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center;">2</td>
                                <td style="border: 1px solid #505050; padding: 8px; color: #f44336;">10% battery remaining</td>
                            </tr>
                            <tr>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center; background-color: #4a4a4a;">22</td>
                                <td style="border: 1px solid #505050; padding: 8px; color: #ff9800;">20% battery remaining</td>
                            </tr>
                            <tr>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center;">222</td>
                                <td style="border: 1px solid #505050; padding: 8px; color: #ff9800;">30% battery remaining</td>
                            </tr>
                            <tr>
                                <td style="border: 1px solid #505050; padding: 8px; text-align: center; background-color: #4a4a4a;">n×2</td>
                                <td style="border: 1px solid #505050; padding: 8px; color: #4fc3f7;">Battery level in 10% steps (4 clicks command)</td>
                            </tr>
                        </tbody>
                    </table>
                </div>
                
                <!-- Project Links -->
                <div class="settings-group">
                    <h3>Project Information</h3>
                    <table class="settings-table">
                        <tr>
                            <td>GitHub Repository:</td>
                            <td>
                                <a href="https://github.com/BubTec/DPVControl" 
                                   target="_blank" 
                                   style="color: #4fc3f7; text-decoration: none;">
                                    https://github.com/BubTec/DPVControl
                                </a>
                            </td>
                        </tr>
                        <tr>
                            <td>Project Blog:</td>
                            <td>
                                <a href="https://bubtec.de/2024/10/29/diy-scooter-dpv-mit-aquazepp-und-dpvcontrol/" 
                                   target="_blank" 
                                   style="color: #4fc3f7; text-decoration: none;">
                                    BubTec DPV Blog Post
                                </a>
                            </td>
                        </tr>
                        <tr>
                            <td>Hardware Platform:</td>
                            <td style="color: #4fc3f7;">ESP32 with VESC Motor Controller</td>
                        </tr>
                        <tr>
                            <td>Project Type:</td>
                            <td style="color: #4fc3f7;">Open Source DIY Diver Propulsion Vehicle</td>
                        </tr>
                    </table>
                </div>
            </div>
        </div>
    </div>

    <script>
        // Variables
        let updateInterval = 10000; // Fixed 10 seconds
        let charts = {};
        let allDataPoints = [];
        let timeSliderValue = 100;
        let systemStartTime = null;
        let availableSessions = [];
        let selectedSession = null; // No "all sessions" option
        let currentTimeRange = 'recent'; // Default time range
        const FIXED_TIME_RANGE_MINUTES = 5; // Fixed 5-minute window
        const FIXED_UPDATE_INTERVAL_MS = 10000; // Fixed 10-second updates
        
        // Initialize the application
        document.addEventListener('DOMContentLoaded', function() {
            // Initialize charts
            initCharts();
            
            // First data load
            loadData();
            
            // Set up periodic updates with enhanced live session support
            setInterval(loadDataWithLiveSession, FIXED_UPDATE_INTERVAL_MS);
            
            // Initialize settings tab
            updateLampBrightnessInputs();
            loadDPVSettings();
            
            // Load available sessions
            loadSessionList();
        });
        
        // Initialize Charts
        function initCharts() {
            // Combined Chart with all data
            const combinedCtx = document.getElementById('combinedChart').getContext('2d');
            charts.combinedChart = new Chart(combinedCtx, {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [{
                        label: 'Battery Voltage (V)',
                        borderColor: 'rgb(75, 192, 192)',
                        backgroundColor: 'rgba(75, 192, 192, 0.1)',
                        data: [],
                        tension: 0.1
                    }, {
                        label: 'Current (A)',
                        borderColor: 'rgb(255, 99, 132)',
                        backgroundColor: 'rgba(255, 99, 132, 0.1)',
                        data: [],
                        tension: 0.1
                    }, {
                        label: 'Motor Temp (°C)',
                        borderColor: 'rgb(255, 206, 86)',
                        backgroundColor: 'rgba(255, 206, 86, 0.1)',
                        data: [],
                        tension: 0.1
                    }, {
                        label: 'Ambient Temp (°C)',
                        borderColor: 'rgb(54, 162, 235)',
                        backgroundColor: 'rgba(54, 162, 235, 0.1)',
                        data: [],
                        tension: 0.1
                    }, {
                        label: 'Humidity (%)',
                        borderColor: 'rgb(153, 102, 255)',
                        backgroundColor: 'rgba(153, 102, 255, 0.1)',
                        data: [],
                        tension: 0.1
                    }, {
                        label: 'RPM (÷100)',
                        borderColor: 'rgb(255, 159, 64)',
                        backgroundColor: 'rgba(255, 159, 64, 0.1)',
                        data: [],
                        tension: 0.1
                    }, {
                        label: 'Duty Cycle (%)',
                        borderColor: 'rgb(199, 199, 199)',
                        backgroundColor: 'rgba(199, 199, 199, 0.1)',
                        data: [],
                        tension: 0.1
                    }, {
                        label: 'MOSFET Temp (°C)',
                        borderColor: 'rgb(255, 99, 255)',
                        backgroundColor: 'rgba(255, 99, 255, 0.1)',
                        data: [],
                        tension: 0.1
                    }, {
                        label: 'Motor Current (A)',
                        borderColor: 'rgb(99, 255, 132)',
                        backgroundColor: 'rgba(99, 255, 132, 0.1)',
                        data: [],
                        tension: 0.1
                    }, {
                        label: 'Battery Level (%)',
                        borderColor: 'rgb(255, 206, 132)',
                        backgroundColor: 'rgba(255, 206, 132, 0.1)',
                        data: [],
                        tension: 0.1
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    scales: {
                        y: {
                            beginAtZero: true,
                            max: 100
                        }
                    },
                    plugins: {
                        legend: {
                            display: true,
                            position: 'top'
                        }
                    }
                }
            });
        }
        
        // Tab Navigation
        function showTab(tabName) {
            // Hide all tabs
            document.querySelectorAll('.tab-content').forEach(tab => {
                tab.classList.remove('active');
            });
            
            // Show selected tab
            document.getElementById(tabName + '-tab').classList.add('active');
            
            // Update active state of buttons
            document.querySelectorAll('.nav-tab').forEach(btn => {
                btn.classList.remove('active');
            });
            
            // Find the button that was clicked and make it active
            event.target.classList.add('active');
        }
        
        // Load available sessions from API
        function loadSessionList() {
            fetch('/api/sessions')
                .then(response => response.json())
                .then(sessions => {
                    updateSessionDropdown(sessions);
                    if (sessions.length > 0) {
                        // Sort sessions and select the newest one (highest number)
                        const sortedSessions = sessions.sort((a, b) => b.filename.localeCompare(a.filename));
                        selectedSession = sortedSessions[0].filename; // Select newest session by default
                        document.getElementById('sessionSelect').value = selectedSession;
                        console.log('Auto-selected newest session:', selectedSession);
                        // Only load chart after session is properly selected
                        setTimeout(() => {
                            refreshChart();
                        }, 100);
                    }
                })
                .catch(error => {
                    console.error('Error loading sessions:', error);
                    selectedSession = null; // Clear selection on error
                });
        }
        
        // Update session dropdown with available sessions
        function updateSessionDropdown(sessions) {
            const sessionSelect = document.getElementById('sessionSelect');
            sessionSelect.innerHTML = '';
            
            // Sort sessions by filename (newest first)
            sessions.sort((a, b) => b.filename.localeCompare(a.filename));
            
            sessions.forEach(session => {
                const option = document.createElement('option');
                option.value = session.filename;
                // Extract session number and make it more readable
                const sessionNumber = session.filename.replace('session_', '').replace('.bin', '');
                let displayText = `Session ${sessionNumber}`;
                if (session.isCurrent) {
                    displayText += ' (Current)';
                }
                option.textContent = displayText;
                sessionSelect.appendChild(option);
            });
            
            // Store complete session objects for live updates
            availableSessions = sessions;
            console.log('Updated session dropdown with', sessions.length, 'sessions');
            
            // Ensure selected session is visible in dropdown
            if (selectedSession && sessionSelect.value !== selectedSession) {
                sessionSelect.value = selectedSession;
            }
        }
        
        // Handle session selection change
        function updateSessionFilter() {
            selectedSession = document.getElementById('sessionSelect').value;
            console.log('Session changed to:', selectedSession);
            refreshChart();
        }
        
        // Update time window based on slider
        function updateTimeWindow() {
            timeSliderValue = parseInt(document.getElementById('timeSlider').value);
            refreshChart();
        }
        
        // Refresh chart with current settings
        function refreshChart() {
            loadChartData();
        }
        
        // Calculate data points needed for 5-minute window
        function getDataPointsForTimeRange() {
            // 5 minutes at 5-second intervals = 60 data points
            return Math.ceil((FIXED_TIME_RANGE_MINUTES * 60) / 5);
        }
        
        // Filter data based on time window slider position
        function filterDataByTimeRange(data) {
            // For session data, we don't need session filtering since each session is loaded separately
            // Just apply the time window based on slider position
            
            const pointsNeeded = getDataPointsForTimeRange();
            if (data.length <= pointsNeeded) {
                return data;
            }
            
            // Calculate window position based on slider
            const maxStart = data.length - pointsNeeded;
            const startIndex = Math.floor((maxStart * (100 - timeSliderValue)) / 100);
            const endIndex = startIndex + pointsNeeded;
            
            // Update slider labels
            if (data.length > 0) {
                const startTime = new Date(data[startIndex].timestamp).toLocaleTimeString();
                const endTime = new Date(data[Math.min(endIndex - 1, data.length - 1)].timestamp).toLocaleTimeString();
                document.getElementById('sliderStart').textContent = startTime;
                document.getElementById('sliderEnd').textContent = endTime;
            }
            
            return data.slice(startIndex, endIndex);
        }
        
        // Format duration in human readable format
        function formatDuration(seconds) {
            const hours = Math.floor(seconds / 3600);
            const minutes = Math.floor((seconds % 3600) / 60);
            const secs = seconds % 60;
            
            if (hours > 0) {
                return `${hours}h ${minutes}m`;
            } else if (minutes > 0) {
                return `${minutes}m ${secs}s`;
            } else {
                return `${secs}s`;
            }
        }
        

        
        // Handle session filter change
        function updateSessionFilter() {
            selectedSession = document.getElementById('sessionSelect').value;
            console.log('Selected session:', selectedSession);
            refreshChart();
        }
        
        // Load data from the API
        function loadData() {
            console.log('loadData called');
            // Fetch status data
            fetch('/api/status')
                .then(response => {
                    console.log('Status response received:', response.status);
                    return response.json();
                })
                .then(data => {
                    console.log('Status data:', data);
                    document.getElementById('uptime').textContent = formatTime(data.uptime);
                    document.getElementById('totalUptime').textContent = formatTime(data.totalUptime * 1000);
                    document.getElementById('dataPointCount').textContent = data.dataPoints || 0;
                    
                    // Update additional sensor data
                    document.getElementById('waterSensorFront').textContent = data.waterSensorFront === 'true' ? 'LEAK DETECTED!' : 'OK';
                    document.getElementById('waterSensorBack').textContent = data.waterSensorBack === 'true' ? 'LEAK DETECTED!' : 'OK';
                                    document.getElementById('leftButton').textContent = data.leftButton === 'true' ? 'PRESSED' : 'RELEASED';
                    document.getElementById('rightButton').textContent = data.rightButton === 'true' ? 'PRESSED' : 'RELEASED';
                    document.getElementById('lampLevel').textContent = 'Level ' + data.lampLevel;
                    document.getElementById('beeperStatus').textContent = data.beeperEnabled === 'true' ? 'Enabled' : 'Disabled';
                    
                    // Update beeper setting checkbox (new settings tab)
                    if (document.getElementById('beeperEnabled')) {
                        document.getElementById('beeperEnabled').checked = data.beeperEnabled === 'true';
                    }
                })
                .catch(error => {
                    console.error('Error fetching status:', error);
                    document.getElementById('uptime').textContent = 'Error loading';
                });
            
            // Fetch latest data point for status display
            fetch('/api/data?count=1')
                .then(response => {
                    console.log('Data response received:', response.status);
                    return response.json();
                })
                .then(data => {
                    console.log('Latest data point:', data);
                    if (data.length > 0) {
                        const latest = data[0];
                        document.getElementById('battery').textContent = latest.batteryVoltage.toFixed(2) + ' V';
                        document.getElementById('motorTemp').textContent = latest.tempMotor.toFixed(1) + ' °C';
                        document.getElementById('mosfetTemp').textContent = latest.tempMosfet.toFixed(1) + ' °C';
                        document.getElementById('temperature').textContent = latest.temperature.toFixed(1) + ' °C';
                        document.getElementById('humidity').textContent = latest.humidity.toFixed(1) + ' %';
                        document.getElementById('current').textContent = latest.current.toFixed(2) + ' A';
                        document.getElementById('motorCurrent').textContent = latest.avgMotorCurrent.toFixed(2) + ' A';
                        document.getElementById('rpm').textContent = latest.rpm.toFixed(0) + ' RPM';
                        document.getElementById('dutyCycle').textContent = latest.dutyCycle.toFixed(1) + ' %';
                        document.getElementById('batteryLevel').textContent = latest.batteryLevel + ' %';
                        document.getElementById('waterSensorFront').textContent = latest.leakSensorState === 1 ? 'LEAK DETECTED!' : 'OK';
                    }
                })
                .catch(error => {
                    console.error('Error fetching data:', error);
                    document.getElementById('battery').textContent = 'Error loading';
                });
            
            // Load chart data separately
            loadChartData();
            
            // Load version info separately
            loadVersionInfo();
        }
        
        // Load version information
        function loadVersionInfo() {
            fetch('/api/version')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('currentVersion').textContent = data.version;
                })
                .catch(error => {
                    console.error('Error fetching version:', error);
                    document.getElementById('currentVersion').textContent = 'Unknown';
                });
            
            // Update system uptime and total runtime from status data
            fetch('/api/status')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('systemUptime').textContent = formatTime(data.uptime);
                    document.getElementById('totalRuntime').textContent = formatTime(data.totalUptime * 1000);
                })
                .catch(error => {
                    console.error('Error fetching uptime:', error);
                    document.getElementById('systemUptime').textContent = 'Unknown';
                    document.getElementById('totalRuntime').textContent = 'Unknown';
                });
        }
        
        // Load chart data from selected session
        function loadChartData() {
            if (!selectedSession || selectedSession === 'undefined' || selectedSession === 'null') {
                console.log('No valid session selected, waiting for session list to load');
                return;
            }
            
            let apiUrl = '/api/session-data?session=' + encodeURIComponent(selectedSession);
            
            console.log('Fetching chart data from:', apiUrl);
            fetch(apiUrl)
                .then(response => {
                    console.log('Chart data response:', response.status);
                    if (!response.ok) {
                        throw new Error(`HTTP ${response.status}`);
                    }
                    return response.json();
                })
                .then(data => {
                    console.log('Chart data received:', data.length, 'points');
                    allDataPoints = data;
                    updateCharts(data);
                })
                .catch(error => {
                    console.error('Error fetching chart data:', error);
                    // Show empty chart on error
                    allDataPoints = [];
                    updateCharts([]);
                });
        }
        
        // Check if selected session is the current (active) session
        function isCurrentSession(sessionFilename) {
            // Find session in availableSessions array that matches current selection
            if (!availableSessions || availableSessions.length === 0) return false;
            
            for (let i = 0; i < availableSessions.length; i++) {
                if (availableSessions[i].filename === sessionFilename && availableSessions[i].isCurrent) {
                    return true;
                }
            }
            return false;
        }
        
        // Enhanced loadData function with live session updates
        function loadDataWithLiveSession() {
            // Always load status and latest data point
            loadData();
            
            // If current session is selected, also reload chart data for live updates
            if (selectedSession && isCurrentSession(selectedSession)) {
                console.log('Live updating current session:', selectedSession);
                loadChartData();
            }
        }
        
        // Update charts with new data
        function updateCharts(data) {
            console.log('updateCharts called with', data.length, 'data points');
            if (data.length === 0) {
                console.log('No data to display in charts');
                return;
            }
            
            // Filter data based on time range and slider
            const filteredData = filterDataByTimeRange(data);
            console.log('Filtered data:', filteredData.length, 'points');
            
            // Prepare labels (timestamps)
            const labels = filteredData.map(item => {
                const date = new Date(item.timestamp);
                return date.toLocaleTimeString();
            });
            console.log('Generated', labels.length, 'labels');
            
            // Update Combined Chart
            charts.combinedChart.data.labels = labels;
            charts.combinedChart.data.datasets[0].data = filteredData.map(item => item.batteryVoltage);
            charts.combinedChart.data.datasets[1].data = filteredData.map(item => item.current);
            charts.combinedChart.data.datasets[2].data = filteredData.map(item => item.tempMotor);
            charts.combinedChart.data.datasets[3].data = filteredData.map(item => item.temperature);
            charts.combinedChart.data.datasets[4].data = filteredData.map(item => item.humidity);
            charts.combinedChart.data.datasets[5].data = filteredData.map(item => item.rpm / 100);
            charts.combinedChart.data.datasets[6].data = filteredData.map(item => item.dutyCycle);
            charts.combinedChart.data.datasets[7].data = filteredData.map(item => item.tempMosfet);
            charts.combinedChart.data.datasets[8].data = filteredData.map(item => item.avgMotorCurrent);
            charts.combinedChart.data.datasets[9].data = filteredData.map(item => item.batteryLevel);
            
            // TODO: Add restart markers later when annotation plugin is working
            console.log('Chart updated with', filteredData.length, 'data points');
            charts.combinedChart.update();
        }
        
        // Load recent data for the data tab
        function loadRecentData() {
            dataPointsToShow = parseInt(document.getElementById('dataPoints').value);
            localStorage.setItem('dataPoints', dataPointsToShow);
            
            fetch('/api/data?count=' + dataPointsToShow)
                .then(response => response.json())
                .then(data => {
                    displayDataTable(data);
                })
                .catch(error => {
                    console.error('Error fetching recent data:', error);
                    document.getElementById('dataDisplay').innerHTML = '<p style="color: red;">Error loading data</p>';
                });
        }
        
        // Display data in table format
        function displayDataTable(data) {
            if (data.length === 0) {
                document.getElementById('dataDisplay').innerHTML = '<p>No data available</p>';
                return;
            }
            
            let html = '<table style="width: 100%; border-collapse: collapse;">';
            html += '<tr style="background-color: #f0f0f0;">';
            html += '<th style="border: 1px solid #ddd; padding: 8px;">Time</th>';
            html += '<th style="border: 1px solid #ddd; padding: 8px;">Battery (V)</th>';
            html += '<th style="border: 1px solid #ddd; padding: 8px;">Bat Level (%)</th>';
            html += '<th style="border: 1px solid #ddd; padding: 8px;">Input Current (A)</th>';
            html += '<th style="border: 1px solid #ddd; padding: 8px;">Motor Current (A)</th>';
            html += '<th style="border: 1px solid #ddd; padding: 8px;">Motor Temp (°C)</th>';
            html += '<th style="border: 1px solid #ddd; padding: 8px;">MOSFET Temp (°C)</th>';
            html += '<th style="border: 1px solid #ddd; padding: 8px;">Ambient Temp (°C)</th>';
            html += '<th style="border: 1px solid #ddd; padding: 8px;">Humidity (%)</th>';
            html += '<th style="border: 1px solid #ddd; padding: 8px;">RPM</th>';
            html += '<th style="border: 1px solid #ddd; padding: 8px;">Duty (%)</th>';
            html += '<th style="border: 1px solid #ddd; padding: 8px;">Water</th>';
            html += '</tr>';
            
            data.reverse().forEach(item => {
                const time = new Date(item.timestamp).toLocaleTimeString();
                html += '<tr>';
                html += '<td style="border: 1px solid #ddd; padding: 8px;">' + time + '</td>';
                html += '<td style="border: 1px solid #ddd; padding: 8px;">' + item.batteryVoltage.toFixed(2) + '</td>';
                html += '<td style="border: 1px solid #ddd; padding: 8px;">' + item.batteryLevel + '</td>';
                html += '<td style="border: 1px solid #ddd; padding: 8px;">' + item.current.toFixed(2) + '</td>';
                html += '<td style="border: 1px solid #ddd; padding: 8px;">' + item.avgMotorCurrent.toFixed(2) + '</td>';
                html += '<td style="border: 1px solid #ddd; padding: 8px;">' + item.tempMotor.toFixed(1) + '</td>';
                html += '<td style="border: 1px solid #ddd; padding: 8px;">' + item.tempMosfet.toFixed(1) + '</td>';
                html += '<td style="border: 1px solid #ddd; padding: 8px;">' + item.temperature.toFixed(1) + '</td>';
                html += '<td style="border: 1px solid #ddd; padding: 8px;">' + item.humidity.toFixed(1) + '</td>';
                html += '<td style="border: 1px solid #ddd; padding: 8px;">' + item.rpm.toFixed(0) + '</td>';
                html += '<td style="border: 1px solid #ddd; padding: 8px;">' + item.dutyCycle.toFixed(1) + '</td>';
                html += '<td style="border: 1px solid #ddd; padding: 8px;">' + (item.leakSensorState === 1 ? 'LEAK!' : 'OK') + '</td>';
                html += '</tr>';
            });
            
            html += '</table>';
            document.getElementById('dataDisplay').innerHTML = html;
        }
        
        // Format time in HH:MM:SS
        function formatTime(milliseconds) {
            const totalSeconds = Math.floor(milliseconds / 1000);
            const hours = Math.floor(totalSeconds / 3600);
            const minutes = Math.floor((totalSeconds % 3600) / 60);
            const seconds = totalSeconds % 60;
            
            return String(hours).padStart(2, '0') + ':' + String(minutes).padStart(2, '0') + ':' + String(seconds).padStart(2, '0');
        }
        
        // Update lamp brightness inputs based on number of levels
        function updateLampBrightnessInputs() {
            const maxLevels = parseInt(document.getElementById('lampMaxLevels').value);
            const container = document.getElementById('lampBrightnessContainer');
            
            container.innerHTML = '<table class="settings-table">';
            for (let i = 0; i < maxLevels; i++) {
                container.innerHTML += `
                    <tr>
                        <td><label for="lampBrightness${i}">Level ${i} Brightness:</label></td>
                        <td><input type="number" id="lampBrightness${i}" min="0" max="255" value="0"></td>
                    </tr>
                `;
            }
            container.innerHTML += '</table>';
        }
        
        // Load DPV settings from API
        function loadDPVSettings() {
            document.getElementById('settingsStatus').textContent = 'Loading settings...';
            
            fetch('/api/settings')
                .then(response => response.json())
                .then(data => {
                    // Motor and speed settings
                    document.getElementById('speedSteps').value = data.speedSteps;
                    document.getElementById('standbyDelaySeconds').value = data.standbyDelaySeconds;
                    document.getElementById('batteryPowerMax').value = data.batteryPowerMax;
                    document.getElementById('minSpeedPercent').value = parseFloat(data.minSpeedPercent).toFixed(2);
                    document.getElementById('maxSpeedRpm').value = data.maxSpeedRpm;
                    document.getElementById('speedUpTimeMs').value = data.speedUpTimeMs;
                    document.getElementById('speedDownTimeMs').value = data.speedDownTimeMs;
                    document.getElementById('maxTimeOverloadedMs').value = data.maxTimeOverloadedMs;
                    
                    // Jam detection
                    document.getElementById('jamMin').value = parseFloat(data.jamMin).toFixed(2);
                    document.getElementById('jamDetectionThreshold').value = parseFloat(data.jamDetectionThreshold).toFixed(2);
                    
                    // Battery
                    document.getElementById('cellsInSeries').value = data.cellsInSeries;
                    
                    // LED Bar
                    document.getElementById('ledBarNum').value = data.ledBarNum;
                    document.getElementById('ledBarBrightness').value = data.ledBarBrightness;
                    document.getElementById('ledBarBrightnessSecond').value = data.ledBarBrightnessSecond;
                    document.getElementById('ledFrequency').value = data.ledFrequency;
                    
                    // Lamp settings
                    document.getElementById('lampMaxLevels').value = data.lampMaxLevels;
                    updateLampBrightnessInputs();
                    for (let i = 0; i < data.lampMaxLevels; i++) {
                        document.getElementById('lampBrightness' + i).value = data.lampBrightness[i];
                    }
                    
                    // WiFi
                    document.getElementById('wifiSSID').value = data.wifiSSID;
                    document.getElementById('wifiPassword').value = data.wifiPassword;
                    
                    // System
                    document.getElementById('beeperEnabled').checked = data.beeperEnabled;
                    document.getElementById('standbyBlinkStartMinutes').value = data.standbyBlinkStartMinutes;
                    document.getElementById('standbyBlinkDurationSeconds').value = data.standbyBlinkDurationSeconds;
                    
                    document.getElementById('settingsStatus').textContent = 'Settings loaded successfully';
                })
                .catch(error => {
                    console.error('Error loading settings:', error);
                    document.getElementById('settingsStatus').textContent = 'Error loading settings';
                });
        }
        
        // Save DPV settings to API
        function saveDPVSettings() {
            document.getElementById('settingsStatus').textContent = 'Saving settings...';
            
            // Collect lamp brightness values
            const maxLevels = parseInt(document.getElementById('lampMaxLevels').value);
            const lampBrightness = [];
            for (let i = 0; i < 10; i++) {
                if (i < maxLevels) {
                    lampBrightness[i] = parseInt(document.getElementById('lampBrightness' + i).value) || 0;
                } else {
                    lampBrightness[i] = 0;
                }
            }
            
            const settingsData = {
                speedSteps: parseInt(document.getElementById('speedSteps').value),
                standbyDelaySeconds: parseInt(document.getElementById('standbyDelaySeconds').value),
                batteryPowerMax: parseInt(document.getElementById('batteryPowerMax').value),
                minSpeedPercent: parseFloat(document.getElementById('minSpeedPercent').value),
                maxSpeedRpm: parseFloat(document.getElementById('maxSpeedRpm').value),
                speedUpTimeMs: parseInt(document.getElementById('speedUpTimeMs').value),
                speedDownTimeMs: parseInt(document.getElementById('speedDownTimeMs').value),
                maxTimeOverloadedMs: parseInt(document.getElementById('maxTimeOverloadedMs').value),
                jamMin: parseFloat(document.getElementById('jamMin').value),
                jamDetectionThreshold: parseFloat(document.getElementById('jamDetectionThreshold').value),
                cellsInSeries: parseInt(document.getElementById('cellsInSeries').value),
                ledBarNum: parseInt(document.getElementById('ledBarNum').value),
                ledBarBrightness: parseInt(document.getElementById('ledBarBrightness').value),
                ledBarBrightnessSecond: parseInt(document.getElementById('ledBarBrightnessSecond').value),
                ledFrequency: parseInt(document.getElementById('ledFrequency').value),
                lampMaxLevels: maxLevels,
                lampBrightness: lampBrightness,
                wifiSSID: document.getElementById('wifiSSID').value,
                wifiPassword: document.getElementById('wifiPassword').value,
                beeperEnabled: document.getElementById('beeperEnabled').checked,
                debugLoggingEnabled: document.getElementById('debugLoggingEnabled').checked,
                standbyBlinkStartMinutes: parseInt(document.getElementById('standbyBlinkStartMinutes').value),
                standbyBlinkDurationSeconds: parseInt(document.getElementById('standbyBlinkDurationSeconds').value)
            };
            
            fetch('/api/settings', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify(settingsData)
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    document.getElementById('settingsStatus').textContent = 'Settings saved successfully!';
                } else {
                    document.getElementById('settingsStatus').textContent = 'Error saving settings!';
                }
            })
            .catch(error => {
                console.error('Error saving settings:', error);
                document.getElementById('settingsStatus').textContent = 'Error saving settings!';
            });
        }
        
        // Restore default settings
        function restoreDefaultSettings() {
            if (confirm('Are you sure you want to restore default settings? This will overwrite all current settings.')) {
                document.getElementById('settingsStatus').textContent = 'Restoring defaults...';
                
                fetch('/api/settings/restore', {
                    method: 'POST'
                })
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        document.getElementById('settingsStatus').textContent = 'Default settings restored!';
                        loadDPVSettings(); // Reload settings from server
                    } else {
                        document.getElementById('settingsStatus').textContent = 'Error restoring defaults!';
                    }
                })
                .catch(error => {
                    console.error('Error restoring defaults:', error);
                    document.getElementById('settingsStatus').textContent = 'Error restoring defaults!';
                });
            }
        }
        
        // Export settings to JSON file
        function exportSettings() {
            document.getElementById('settingsStatus').textContent = 'Exporting settings...';
            
            fetch('/api/settings')
                .then(response => response.json())
                .then(data => {
                    // Add metadata to the export
                    const exportData = {
                        exportInfo: {
                            version: "1.0",
                            timestamp: new Date().toISOString(),
                            device: "DPVControl"
                        },
                        settings: data
                    };
                    
                    // Create and download file
                    const blob = new Blob([JSON.stringify(exportData, null, 2)], { type: 'application/json' });
                    const url = URL.createObjectURL(blob);
                    const a = document.createElement('a');
                    a.href = url;
                    
                    const timestamp = new Date().toISOString().slice(0, 19).replace(/:/g, '-');
                    a.download = `dpv_settings_${timestamp}.json`;
                    
                    document.body.appendChild(a);
                    a.click();
                    document.body.removeChild(a);
                    URL.revokeObjectURL(url);
                    
                    document.getElementById('settingsStatus').textContent = 'Settings exported successfully!';
                })
                .catch(error => {
                    console.error('Error exporting settings:', error);
                    document.getElementById('settingsStatus').textContent = 'Error exporting settings!';
                });
        }
        
        // Import settings from JSON file
        function importSettings() {
            document.getElementById('settingsFileInput').click();
        }
        
        // Handle selected settings file
        function handleSettingsFile(event) {
            const file = event.target.files[0];
            if (!file) return;
            
            document.getElementById('settingsStatus').textContent = 'Importing settings...';
            
            const reader = new FileReader();
            reader.onload = function(e) {
                try {
                    const importData = JSON.parse(e.target.result);
                    
                    // Check if it's a valid DPV settings export
                    let settingsToImport;
                    if (importData.exportInfo && importData.settings) {
                        settingsToImport = importData.settings;
                        console.log('Importing settings from:', importData.exportInfo);
                    } else {
                        // Assume it's raw settings data
                        settingsToImport = importData;
                    }
                    
                    // Get current settings to compare
                    fetch('/api/settings')
                        .then(response => response.json())
                        .then(currentSettings => {
                            processSettingsImport(settingsToImport, currentSettings);
                        })
                        .catch(error => {
                            console.error('Error fetching current settings:', error);
                            document.getElementById('settingsStatus').textContent = 'Error importing settings!';
                        });
                        
                } catch (error) {
                    console.error('Error parsing settings file:', error);
                    document.getElementById('settingsStatus').textContent = 'Error: Invalid settings file format!';
                }
            };
            
            reader.readAsText(file);
            
            // Reset file input
            event.target.value = '';
        }
        
        // Process settings import with validation
        function processSettingsImport(importedSettings, currentSettings) {
            const missingFields = [];
            const importedData = {};
            
            // Check each field in current settings
            for (const key in currentSettings) {
                if (importedSettings.hasOwnProperty(key)) {
                    importedData[key] = importedSettings[key];
                } else {
                    missingFields.push(key);
                    // Keep current value for missing fields
                    importedData[key] = currentSettings[key];
                }
            }
            
            // Save the merged settings
            fetch('/api/settings', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify(importedData)
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    let statusMsg = 'Settings imported successfully!';
                    
                    if (missingFields.length > 0) {
                        statusMsg += ` Note: ${missingFields.length} field(s) not found in import file: ${missingFields.join(', ')}. Using current values for these fields.`;
                    }
                    
                    document.getElementById('settingsStatus').textContent = statusMsg;
                    
                    // Reload settings to update UI
                    setTimeout(() => {
                        loadDPVSettings();
                    }, 1000);
                    
                } else {
                    document.getElementById('settingsStatus').textContent = 'Error saving imported settings!';
                }
            })
            .catch(error => {
                console.error('Error saving imported settings:', error);
                document.getElementById('settingsStatus').textContent = 'Error saving imported settings!';
            });
        }
        
        // Remote Control Variables
        let motorRunning = false;
        let currentMotorSpeed = 0;
        let currentLampLevel = 0;
        
        // Toggle motor on/off
        function toggleMotor() {
            motorRunning = !motorRunning;
            const button = document.getElementById('motorToggle');
            
            if (motorRunning) {
                button.textContent = 'STOP MOTOR';
                button.style.backgroundColor = '#f44336';
                setMotorSpeed(currentMotorSpeed);
            } else {
                button.textContent = 'START MOTOR';
                button.style.backgroundColor = '#4caf50';
                setMotorSpeed(0);
            }
            
            updateRemoteStatus();
        }
        
        // Update motor speed display (while dragging)
        function updateMotorSpeed(value) {
            currentMotorSpeed = parseInt(value);
            document.getElementById('motorSpeedValue').textContent = currentMotorSpeed;
            document.getElementById('remoteMotorSpeed').textContent = currentMotorSpeed + '%';
        }
        
        // Set motor speed (when slider is released)
        function setMotorSpeed(value) {
            currentMotorSpeed = parseInt(value);
            updateMotorSpeed(value);
            
            const enabled = motorRunning && currentMotorSpeed > 0;
            const actualSpeed = motorRunning ? currentMotorSpeed : 0;
            
            fetch('/api/motor', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ 
                    enabled: enabled,
                    speed: actualSpeed 
                })
            })
            .then(response => response.json())
            .then(data => {
                console.log('Motor control response:', data);
                document.getElementById('remoteLastCommand').textContent = 
                    `Motor ${enabled ? 'ON' : 'OFF'} @ ${actualSpeed}%`;
                updateRemoteStatus();
            })
            .catch(error => {
                console.error('Error controlling motor:', error);
                document.getElementById('remoteControlStatus').textContent = 'Error controlling motor!';
            });
        }
        
        // Emergency stop
        function emergencyStop() {
            motorRunning = false;
            currentMotorSpeed = 0;
            
            // Reset UI
            document.getElementById('motorToggle').textContent = 'START MOTOR';
            document.getElementById('motorToggle').style.backgroundColor = '#4caf50';
            document.getElementById('motorSpeedSlider').value = 0;
            updateMotorSpeed(0);
            
            // Send stop command
            fetch('/api/motor', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ 
                    enabled: false,
                    speed: 0 
                })
            })
            .then(response => response.json())
            .then(data => {
                console.log('Emergency stop response:', data);
                document.getElementById('remoteLastCommand').textContent = 'EMERGENCY STOP';
                document.getElementById('remoteControlStatus').textContent = 'Emergency stop executed!';
                updateRemoteStatus();
            })
            .catch(error => {
                console.error('Error with emergency stop:', error);
                document.getElementById('remoteControlStatus').textContent = 'Error with emergency stop!';
            });
        }
        
        // Update lamp level display (while dragging)
        function updateLampLevel(value) {
            currentLampLevel = parseInt(value);
            const levelNames = ['OFF', 'Level 1', 'Level 2', 'Level 3', 'MAX'];
            document.getElementById('lampLevelValue').textContent = currentLampLevel;
            document.getElementById('remoteLampStatus').textContent = levelNames[currentLampLevel] || 'OFF';
        }
        
        // Set lamp level (when slider is released)
        function setLampLevel(value) {
            currentLampLevel = parseInt(value);
            updateLampLevel(value);
            document.getElementById('lampLevelSlider').value = value;
            
            fetch('/api/lamp', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ 
                    level: currentLampLevel 
                })
            })
            .then(response => response.json())
            .then(data => {
                console.log('Lamp control response:', data);
                const levelNames = ['OFF', 'Level 1', 'Level 2', 'Level 3', 'MAX'];
                document.getElementById('remoteLastCommand').textContent = 
                    `Lamp set to ${levelNames[currentLampLevel]}`;
                document.getElementById('remoteControlStatus').textContent = 'Lamp level updated!';
            })
            .catch(error => {
                console.error('Error controlling lamp:', error);
                document.getElementById('remoteControlStatus').textContent = 'Error controlling lamp!';
            });
        }
        
        // Update remote control status display
        function updateRemoteStatus() {
            const statusElement = document.getElementById('remoteMotorStatus');
            if (motorRunning && currentMotorSpeed > 0) {
                statusElement.textContent = 'RUNNING';
                statusElement.style.color = '#4caf50';
            } else if (motorRunning && currentMotorSpeed === 0) {
                statusElement.textContent = 'STANDBY';
                statusElement.style.color = '#ff9800';
            } else {
                statusElement.textContent = 'STOPPED';
                statusElement.style.color = '#f44336';
            }
        }
        
        // Save beeper setting
        function saveBeeperSetting() {
            const enabled = document.getElementById('beeperEnabledSetting').checked;
            
            fetch('/api/beeper', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ enabled: enabled })
            })
            .then(response => response.json())
            .then(data => {
                console.log('Beeper setting saved:', data);
                // Update status display immediately
                document.getElementById('beeperStatus').textContent = enabled ? 'Enabled' : 'Disabled';
            })
            .catch(error => {
                console.error('Error saving beeper setting:', error);
                alert('Failed to save beeper setting');
            });
        }
        
        // Export current view as CSV
        function exportCurrentViewAsCSV() {
            if (!allDataPoints || allDataPoints.length === 0) {
                alert('No data available for export');
                return;
            }
            
            const filteredData = filterDataByTimeRange(allDataPoints);
            let filename = 'current_view';
            
            // Add session info to filename if specific session is selected
            if (selectedSession) {
                const sessionNumber = selectedSession.replace('session_', '').replace('.bin', '');
                filename = `session_${sessionNumber}`;
            }
            
            exportDataToCSV(filteredData, filename);
        }
        
        // Export full trip log as CSV
        async function exportFullTripLogAsCSV() {
            try {
                // Show loading indicator
                const button = event.target;
                const originalText = button.textContent;
                button.textContent = 'Downloading...';
                button.disabled = true;
                
                // Fetch all trip data
                const response = await fetch('/api/trip-log');
                if (!response.ok) {
                    throw new Error(`HTTP error! status: ${response.status}`);
                }
                
                const fullTripData = await response.json();
                
                if (!fullTripData || fullTripData.length === 0) {
                    alert('No trip data available for download');
                    return;
                }
                
                exportDataToCSV(fullTripData, 'full_trip_log');
                
            } catch (error) {
                console.error('Error downloading full trip log:', error);
                alert('Failed to download trip log: ' + error.message);
            } finally {
                // Restore button
                const button = event.target;
                button.textContent = originalText;
                button.disabled = false;
            }
        }
        
        // Export data to CSV file
        function exportDataToCSV(dataPoints, filePrefix) {
            // Create CSV header
            const headers = [
                'Timestamp',
                'Motor Temperature (°C)',
                'MOSFET Temperature (°C)', 
                'Battery Voltage (V)',
                'Input Current (A)',
                'Motor Current (A)',
                'RPM',
                'Duty Cycle (%)',
                'Ambient Temperature (°C)',
                'Humidity (%)',
                'Battery Level (%)',
                'Leak Sensor State',
                'LED State',
                'Total Uptime (s)'
            ];
            
            // Create CSV content
            let csvContent = headers.join(',') + '\n';
            
            dataPoints.forEach(point => {
                const row = [
                    new Date(point.timestamp).toISOString(),
                    point.tempMotor || 0,
                    point.tempMosfet || 0,
                    point.batteryVoltage || 0,
                    point.current || 0,
                    point.avgMotorCurrent || 0,
                    point.rpm || 0,
                    point.dutyCycle || 0,
                    point.temperature || 0,
                    point.humidity || 0,
                    point.batteryLevel || 0,
                    point.leakSensorState || 0,
                    point.ledState || 0,
                    point.totalUptime || 0
                ];
                csvContent += row.join(',') + '\n';
            });
            
            // Create and download file
            const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
            const link = document.createElement('a');
            const url = URL.createObjectURL(blob);
            link.setAttribute('href', url);
            
            const timestamp = new Date().toISOString().slice(0, 19).replace(/:/g, '-');
            const filename = `dpv_${filePrefix}_${timestamp}.csv`;
            link.setAttribute('download', filename);
            
            link.style.visibility = 'hidden';
            document.body.appendChild(link);
            link.click();
            document.body.removeChild(link);
            
            // Show success message
            const pointCount = dataPoints.length;
            alert(`Successfully exported ${pointCount} data points to ${filename}`);
        }
        
        // Delete all sessions with confirmation
        function deleteAllSessions() {
            const confirmMessage = 'Are you sure you want to DELETE ALL SESSION FILES?\\n\\n' +
                                 'This action cannot be undone!\\n\\n' +
                                 'Type "DELETE ALL" to confirm:';
            
            const userInput = prompt(confirmMessage);
            
            if (userInput === 'DELETE ALL') {
                fetch('/api/delete-all-sessions', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json',
                    }
                })
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        alert('All sessions have been deleted successfully!');
                        // Reload session list
                        loadSessionList();
                    } else {
                        alert('Error deleting sessions: ' + (data.error || 'Unknown error'));
                    }
                })
                .catch(error => {
                    console.error('Error deleting sessions:', error);
                    alert('Failed to delete sessions: ' + error.message);
                });
            } else if (userInput !== null) {
                alert('Deletion cancelled. You must type "DELETE ALL" exactly to confirm.');
            }
        }
    </script>
</body>
</html>
)rawliteral";

// Helper function to send a HTTP response
void sendHttpResponse(WiFiClient client, int statusCode, const char* contentType, const char* content) {
    client.print("HTTP/1.1 ");
    client.print(statusCode);
    client.print(" ");
    
    // Status message based on code
    switch(statusCode) {
        case 200: client.println("OK"); break;
        case 302: client.println("Found"); break;
        case 404: client.println("Not Found"); break;
        default: client.println("OK");
    }
    
    client.print("Content-Type: ");
    client.println(contentType);
    
    if (statusCode == 302) {
        client.print("Location: http://");
        client.println(apIP.toString());
        client.println("Cache-Control: no-cache, no-store, must-revalidate");
        client.println("Pragma: no-cache");
        client.println("Expires: -1");
    }
    
    client.print("Content-Length: ");
    client.println(strlen(content));
    client.println("Connection: close");
    client.println();
    client.println(content);
}

// Helper function to load file from SPIFFS and send to client
bool loadFromSPIFFS(WiFiClient client, String path) {
    String dataType = "text/plain";
    
    // Set the correct dataType based on file extension
    if (path.endsWith(".html")) dataType = "text/html";
    else if (path.endsWith(".css")) dataType = "text/css";
    else if (path.endsWith(".js")) dataType = "application/javascript";
    else if (path.endsWith(".png")) dataType = "image/png";
    else if (path.endsWith(".jpg")) dataType = "image/jpeg";
    else if (path.endsWith(".ico")) dataType = "image/x-icon";
    
    // Open the file
    File dataFile = LittleFS.open(path.c_str(), "r");
    
    if (!dataFile) {
        log("Failed to open file");
        return false;
    }
    
    // HTTP response header
    client.println("HTTP/1.1 200 OK");
    client.print("Content-Type: ");
    client.println(dataType);
    client.println("Connection: close");
    client.println();
    
    // Stream file to client
    byte buffer[64];
    int bytesRead;
    
    while ((bytesRead = dataFile.read(buffer, sizeof(buffer))) > 0) {
        client.write(buffer, bytesRead);
    }
    
    // Close the file
    dataFile.close();
    return true;
}

// Helper function to generate JSON data from datalogger data
String generateDataLoggerJson(int count, String timeRange = "recent") {
    log("generateDataLoggerJson called");
    String countMsg = "Requested count: " + String(count) + ", range: " + timeRange + ", available: " + String(getTotalDataPoints(timeRange));
    log(countMsg.c_str());
    
    LogdataRow* dataPoints = getLatestDataPoints(count, timeRange);
    
    // Wenn keine Daten verfügbar sind, gib leeres Array zurück
    if (!dataPoints || getTotalDataPoints(timeRange) == 0) {
        log("No data available, returning empty array");
        return "[]";
    }
    
    log("Building JSON from real data");
    String json = "[";
    int actualCount = count < getTotalDataPoints(timeRange) ? count : getTotalDataPoints(timeRange);
    
    String actualCountMsg = "Building JSON with " + String(actualCount) + " data points from " + timeRange + " range";
    log(actualCountMsg.c_str());
    
    for (int i = 0; i < actualCount; i++) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"timestamp\":" + String(dataPoints[i].timestamp) + ",";
        json += "\"tempMotor\":" + String(dataPoints[i].tempMotor) + ",";
        json += "\"tempMosfet\":" + String(dataPoints[i].tempMosfet) + ",";
        json += "\"batteryVoltage\":" + String(dataPoints[i].batteryVoltage) + ",";
        json += "\"current\":" + String(dataPoints[i].current) + ",";
        json += "\"avgMotorCurrent\":" + String(dataPoints[i].avgMotorCurrent) + ",";
        json += "\"rpm\":" + String(dataPoints[i].rpm) + ",";
        json += "\"dutyCycle\":" + String(dataPoints[i].dutyCycle) + ",";
        json += "\"temperature\":" + String(dataPoints[i].temperature) + ",";
        json += "\"humidity\":" + String(dataPoints[i].humidity) + ",";
        json += "\"batteryLevel\":" + String(dataPoints[i].batteryLevel) + ",";
        json += "\"leakSensorState\":" + String(dataPoints[i].leakSensorState) + ",";
        json += "\"ledState\":" + String(dataPoints[i].ledState) + ",";
        json += "\"totalUptime\":" + String(dataPoints[i].totalUptime);
        json += "}";
    }
    json += "]";
    
    String jsonLengthMsg = "Generated JSON length: " + String(json.length());
    log(jsonLengthMsg.c_str());
    
    // Debug: Zeige ersten Teil des JSON
    if (json.length() > 100) {
        String jsonPreview = "JSON preview: " + json.substring(0, 100) + "...";
        log(jsonPreview.c_str());
    } else {
        String jsonFull = "JSON full: " + json;
        log(jsonFull.c_str());
    }
    
    return json;
}

/**
 * Generate JSON for complete trip log from LittleFS file
 */
String generateFullTripLogJson() {
    log("generateFullTripLogJson called");
    
    if (!LittleFS.exists("/trip_log.bin")) {
        log("No trip log file found");
        return "[]";
    }
    
    File tripFile = LittleFS.open("/trip_log.bin", "r");
    if (!tripFile) {
        log("Failed to open trip log file");
        return "[]";
    }
    
    size_t fileSize = tripFile.size();
    size_t dataPointCount = fileSize / sizeof(LogdataRow);
    
    String countMsg = "Trip log contains " + String(dataPointCount) + " data points (" + String(fileSize) + " bytes)";
    log(countMsg.c_str());
    
    if (dataPointCount == 0) {
        tripFile.close();
        return "[]";
    }
    
    String json = "[";
    LogdataRow dataPoint;
    bool firstPoint = true;
    
    // Read and convert each data point
    for (size_t i = 0; i < dataPointCount; i++) {
        size_t bytesRead = tripFile.read((uint8_t*)&dataPoint, sizeof(LogdataRow));
        
        if (bytesRead != sizeof(LogdataRow)) {
            String errorMsg = "Error reading data point " + String(i) + ", bytes read: " + String(bytesRead);
            log(errorMsg.c_str());
            break;
        }
        
        if (!firstPoint) json += ",";
        firstPoint = false;
        
        json += "{";
        json += "\"timestamp\":" + String(dataPoint.timestamp) + ",";
        json += "\"tempMotor\":" + String(dataPoint.tempMotor) + ",";
        json += "\"tempMosfet\":" + String(dataPoint.tempMosfet) + ",";
        json += "\"batteryVoltage\":" + String(dataPoint.batteryVoltage) + ",";
        json += "\"current\":" + String(dataPoint.current) + ",";
        json += "\"avgMotorCurrent\":" + String(dataPoint.avgMotorCurrent) + ",";
        json += "\"rpm\":" + String(dataPoint.rpm) + ",";
        json += "\"dutyCycle\":" + String(dataPoint.dutyCycle) + ",";
        json += "\"temperature\":" + String(dataPoint.temperature) + ",";
        json += "\"humidity\":" + String(dataPoint.humidity) + ",";
        json += "\"batteryLevel\":" + String(dataPoint.batteryLevel) + ",";
        json += "\"leakSensorState\":" + String(dataPoint.leakSensorState) + ",";
        json += "\"ledState\":" + String(dataPoint.ledState) + ",";
        json += "\"totalUptime\":" + String(dataPoint.totalUptime);
        json += "}";
        
        // Prevent memory overflow for very large files
        if (json.length() > 50000) { // Limit to ~50KB JSON
            String limitMsg = "JSON size limit reached at " + String(i+1) + " points, truncating";
            log(limitMsg.c_str());
            break;
        }
    }
    
    json += "]";
    tripFile.close();
    
    String resultMsg = "Generated full trip log JSON, length: " + String(json.length()) + " for " + String(dataPointCount) + " points";
    log(resultMsg.c_str());
    
    return json;
}

/**
 * Generate JSON for current DPV settings
 */
String generateSettingsJson() {
    log("generateSettingsJson called");
    
    DynamicJsonDocument doc(2048);
    
    // Motor and speed settings
    doc["speedSteps"] = currentSettings.speedSteps;
    doc["standbyDelaySeconds"] = currentSettings.standbyDelaySeconds;
    doc["batteryPowerMax"] = currentSettings.batteryPowerMax;
    doc["minSpeedPercent"] = currentSettings.minSpeedPercent;
    doc["maxSpeedRpm"] = currentSettings.maxSpeedRpm;
    doc["speedUpTimeMs"] = currentSettings.speedUpTimeMs;
    doc["speedDownTimeMs"] = currentSettings.speedDownTimeMs;
    doc["maxTimeOverloadedMs"] = currentSettings.maxTimeOverloadedMs;
    
    // Jam detection
    doc["jamMin"] = currentSettings.jamMin;
    doc["jamDetectionThreshold"] = currentSettings.jamDetectionThreshold;
    
    // Battery settings
    doc["cellsInSeries"] = currentSettings.cellsInSeries;
    
    // LED Bar settings
    doc["ledBarNum"] = currentSettings.ledBarNum;
    doc["ledBarBrightness"] = currentSettings.ledBarBrightness;
    doc["ledBarBrightnessSecond"] = currentSettings.ledBarBrightnessSecond;
    doc["ledFrequency"] = currentSettings.ledFrequency;
    
    // Lamp settings
    doc["lampMaxLevels"] = currentSettings.lampMaxLevels;
    JsonArray lampBrightness = doc.createNestedArray("lampBrightness");
    for (int i = 0; i < 10; i++) {
        lampBrightness.add(currentSettings.lampBrightness[i]);
    }
    
    // WiFi settings
    doc["wifiSSID"] = currentSettings.wifiSSID;
    doc["wifiPassword"] = currentSettings.wifiPassword;
    
    // System settings
    doc["beeperEnabled"] = currentSettings.beeperEnabled;
    doc["debugLoggingEnabled"] = currentSettings.debugLoggingEnabled;
    doc["standbyBlinkStartMinutes"] = currentSettings.standbyBlinkStartMinutes;
    doc["standbyBlinkDurationSeconds"] = currentSettings.standbyBlinkDurationSeconds;
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    String jsonMsg = "Generated settings JSON, length: " + String(jsonString.length());
    log(jsonMsg.c_str());
    
    return jsonString;
}

/**
 * Update settings from JSON string
 */
bool updateSettingsFromJson(const String& jsonString) {
    log("updateSettingsFromJson called");
    
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        String errorMsg = "Failed to parse settings JSON: " + String(error.c_str());
        log(errorMsg.c_str());
        return false;
    }
    
    // Create temporary settings structure
    DPVSettings newSettings = currentSettings;
    
    // Update settings from JSON
    if (doc.containsKey("speedSteps")) newSettings.speedSteps = doc["speedSteps"];
    if (doc.containsKey("standbyDelaySeconds")) newSettings.standbyDelaySeconds = doc["standbyDelaySeconds"];
    if (doc.containsKey("batteryPowerMax")) newSettings.batteryPowerMax = doc["batteryPowerMax"];
    if (doc.containsKey("minSpeedPercent")) newSettings.minSpeedPercent = doc["minSpeedPercent"];
    if (doc.containsKey("maxSpeedRpm")) newSettings.maxSpeedRpm = doc["maxSpeedRpm"];
    if (doc.containsKey("speedUpTimeMs")) newSettings.speedUpTimeMs = doc["speedUpTimeMs"];
    if (doc.containsKey("speedDownTimeMs")) newSettings.speedDownTimeMs = doc["speedDownTimeMs"];
    if (doc.containsKey("maxTimeOverloadedMs")) newSettings.maxTimeOverloadedMs = doc["maxTimeOverloadedMs"];
    
    if (doc.containsKey("jamMin")) newSettings.jamMin = doc["jamMin"];
    if (doc.containsKey("jamDetectionThreshold")) newSettings.jamDetectionThreshold = doc["jamDetectionThreshold"];
    
    if (doc.containsKey("cellsInSeries")) newSettings.cellsInSeries = doc["cellsInSeries"];
    
    if (doc.containsKey("ledBarNum")) newSettings.ledBarNum = doc["ledBarNum"];
    if (doc.containsKey("ledBarBrightness")) newSettings.ledBarBrightness = doc["ledBarBrightness"];
    if (doc.containsKey("ledBarBrightnessSecond")) newSettings.ledBarBrightnessSecond = doc["ledBarBrightnessSecond"];
    if (doc.containsKey("ledFrequency")) newSettings.ledFrequency = doc["ledFrequency"];
    
    if (doc.containsKey("lampMaxLevels")) newSettings.lampMaxLevels = doc["lampMaxLevels"];
    if (doc.containsKey("lampBrightness")) {
        JsonArray lampArray = doc["lampBrightness"];
        for (int i = 0; i < 10 && i < lampArray.size(); i++) {
            newSettings.lampBrightness[i] = lampArray[i];
        }
    }
    
    if (doc.containsKey("wifiSSID")) {
        strncpy(newSettings.wifiSSID, doc["wifiSSID"], sizeof(newSettings.wifiSSID) - 1);
        newSettings.wifiSSID[sizeof(newSettings.wifiSSID) - 1] = '\0';
    }
    if (doc.containsKey("wifiPassword")) {
        strncpy(newSettings.wifiPassword, doc["wifiPassword"], sizeof(newSettings.wifiPassword) - 1);
        newSettings.wifiPassword[sizeof(newSettings.wifiPassword) - 1] = '\0';
    }
    
    if (doc.containsKey("beeperEnabled")) newSettings.beeperEnabled = doc["beeperEnabled"];
    if (doc.containsKey("debugLoggingEnabled")) newSettings.debugLoggingEnabled = doc["debugLoggingEnabled"];
    if (doc.containsKey("standbyBlinkStartMinutes")) newSettings.standbyBlinkStartMinutes = doc["standbyBlinkStartMinutes"];
    if (doc.containsKey("standbyBlinkDurationSeconds")) newSettings.standbyBlinkDurationSeconds = doc["standbyBlinkDurationSeconds"];
    
    // Validate new settings
    if (!validateSettings(newSettings)) {
        log("New settings failed validation");
        return false;
    }
    
    // Apply new settings
    currentSettings = newSettings;
    saveSettings();
    
    log("Settings updated successfully");
    return true;
}

// Setup the webserver task on Core 0
void setupWebserver() {
    log("Setting up webserver on Core 0");
    
    // Initialize SPIFFS and store HTML files
    spiffsInitialized = initializeFileSystem();
    
    // Create task on Core 0
    xTaskCreatePinnedToCore(
        webserverTask,         // Task function
        "WebserverTask",       // Task name
        10000,                 // Stack size (bytes)
        NULL,                  // Task parameters
        1,                     // Task priority (1 is low)
        &webserverTaskHandle,  // Task handle
        0                      // Core ID (0)
    );
    
    log("Webserver task created on Core 0");
}

// Process HTTP requests
void handleClient(WiFiClient client) {
    // Wait for data to be available
    unsigned long timeout = millis() + 5000; // 5 second timeout
    while (!client.available() && millis() < timeout) {
        delay(10);
    }
    
    // If no data, close connection and return
    if (!client.available()) {
        client.stop();
        return;
    }
    
    // Read the complete HTTP request
    String httpRequest = "";
    String line = "";
    String method = "";
    String path = "";
    String host = "";
    String contentLength = "";
    
    // Read request line
    line = client.readStringUntil('\n');
    httpRequest += line;
    
    // Extract method and path from first line
    int firstSpace = line.indexOf(' ');
    int secondSpace = line.indexOf(' ', firstSpace + 1);
    
    if (firstSpace != -1 && secondSpace != -1) {
        method = line.substring(0, firstSpace);
        path = line.substring(firstSpace + 1, secondSpace);
    }
    
    log(("Request: " + method + " " + path).c_str());
    
    // Read headers
    while (client.connected()) {
        line = client.readStringUntil('\n');
        line.trim();
        httpRequest += line + "\n";
        
        if (line.startsWith("Host: ")) {
            host = line.substring(6);
            log(("Host: " + host).c_str());
        }
        
        if (line.startsWith("Content-Length: ")) {
            contentLength = line.substring(16);
        }
        
        // Empty line indicates end of headers
        if (line.length() == 0) {
            break;
        }
    }
    
    // Check if this is a captive portal detection request
    bool isCaptivePortalRequest = host.length() > 0 && 
                                 !host.equals(apIP.toString()) &&
                                 !host.startsWith("4.3.2.") &&
                                 !host.equals("localhost") &&
                                 !host.equals("captive.apple.com");
    
    // Handle the request based on the path
    if (path == "/" || path == "/index.html") {
        // Root path - serve embedded HTML page
        sendHttpResponse(client, 200, "text/html", helloWorldHTML);
    } else if (path == "/api/data" || path.startsWith("/api/data?")) {
        // API endpoint for datalogger data
        log("API /api/data called");
        
        int count = 60; // Default: return 60 data points
        String timeRange = "recent"; // Default: recent data
        
        // Extract count parameter if present
        if (path.indexOf("count=") != -1) {
            String countStr = path.substring(path.indexOf("count=") + 6);
            if (countStr.indexOf("&") != -1) {
                countStr = countStr.substring(0, countStr.indexOf("&"));
            }
            count = countStr.toInt();
            if (count <= 0 || count > MAX_RECENT_POINTS) {
                count = 60; // Fallback to default
            }
        }
        
        // Extract timeRange parameter if present
        if (path.indexOf("range=") != -1) {
            String rangeStr = path.substring(path.indexOf("range=") + 6);
            if (rangeStr.indexOf("&") != -1) {
                rangeStr = rangeStr.substring(0, rangeStr.indexOf("&"));
            }
            if (rangeStr == "hourly" || rangeStr == "historical") {
                timeRange = rangeStr;
            }
        }
        
        String paramMsg = "API data request - count: " + String(count) + ", range: " + timeRange;
        log(paramMsg.c_str());
        
        // Generate and send JSON data
        String jsonData = generateDataLoggerJson(count, timeRange);
        
        String responseMsg = "Sending JSON response, length: " + String(jsonData.length());
        log(responseMsg.c_str());
        
        sendHttpResponse(client, 200, "application/json", jsonData.c_str());
        
    } else if (path == "/api/status") {
        // API endpoint for system status
        log("API /api/status called");
        
        String jsonStatus = "{";
        jsonStatus += "\"uptime\":" + String(millis()) + ",";
        jsonStatus += "\"totalUptime\":" + String(getTotalUptime()) + ",";
        jsonStatus += "\"dataPoints\":" + String(getTotalDataPoints("recent")) + ",";
        jsonStatus += "\"isDataloggerRunning\":" + String(isDataloggerRunning ? "true" : "false") + ",";
        
        // Additional sensor data
        jsonStatus += "\"waterSensorFront\":" + String(digitalRead(PIN_LEAK_FRONT) == LOW ? "true" : "false") + ",";
        jsonStatus += "\"waterSensorBack\":" + String(digitalRead(PIN_LEAK_BACK) == LOW ? "true" : "false") + ",";
        jsonStatus += "\"leftButton\":" + String(digitalRead(PIN_LEFT_BUTTON) == LOW ? "true" : "false") + ",";
        jsonStatus += "\"rightButton\":" + String(digitalRead(PIN_RIGHT_BUTTON) == LOW ? "true" : "false") + ",";
        jsonStatus += "\"lampLevel\":" + String(LED_State) + ",";
        jsonStatus += "\"beeperEnabled\":" + String(beeperEnabled ? "true" : "false");
        jsonStatus += "}";
        
        String statusMsg = "Status response: " + jsonStatus;
        log(statusMsg.c_str());
        
        sendHttpResponse(client, 200, "application/json", jsonStatus.c_str());
        
    } else if (path == "/api/sessions") {
        // API endpoint for listing all sessions
        log("API /api/sessions called");
        
        String jsonData = generateSessionListJson();
        
        String responseMsg = "Sending session list, length: " + String(jsonData.length());
        log(responseMsg.c_str());
        
        sendHttpResponse(client, 200, "application/json", jsonData.c_str());
        
    } else if (path.startsWith("/api/session-data?")) {
        // API endpoint for getting data from a specific session
        log("API /api/session-data called");
        
        // Extract session parameter
        String sessionFile = "";
        if (path.indexOf("session=") != -1) {
            sessionFile = path.substring(path.indexOf("session=") + 8);
            if (sessionFile.indexOf("&") != -1) {
                sessionFile = sessionFile.substring(0, sessionFile.indexOf("&"));
            }
        }
        
        if (sessionFile == "") {
            sendHttpResponse(client, 400, "text/plain", "Missing session parameter");
            return;
        }
        
        String jsonData = generateSessionDataJson(sessionFile);
        
        String responseMsg = "Sending session data, length: " + String(jsonData.length());
        log(responseMsg.c_str());
        
        sendHttpResponse(client, 200, "application/json", jsonData.c_str());
        
    } else if (path == "/api/trip-log") {
        // API endpoint for full trip log download
        log("API /api/trip-log called");
        
        String jsonData = generateFullTripLogJson();
        
        String responseMsg = "Sending full trip log, length: " + String(jsonData.length());
        log(responseMsg.c_str());
        
        sendHttpResponse(client, 200, "application/json", jsonData.c_str());
        
    } else if (path == "/api/settings" && method == "GET") {
        // API endpoint to get current settings
        log("API /api/settings GET called");
        
        String jsonSettings = generateSettingsJson();
        sendHttpResponse(client, 200, "application/json", jsonSettings.c_str());
        
    } else if (path == "/api/settings" && method == "POST") {
        // API endpoint to save settings
        log("API /api/settings POST called");
        
        // Skip headers first
        while (client.connected() && client.available()) {
            String line = client.readStringUntil('\n');
            line.trim();
            if (line.length() == 0) {
                break; // End of headers
            }
        }
        
        // Wait a bit for body data to arrive
        delay(50);
        
        // Read POST body - all available data
        String body = "";
        while (client.available()) {
            body += (char)client.read();
        }
        
        String bodyMsg = "POST body received, length: " + String(body.length());
        log(bodyMsg.c_str());
        
        if (body.length() > 50) {
            String bodyPreview = "POST body preview: " + body.substring(0, 50) + "...";
            log(bodyPreview.c_str());
        } else if (body.length() > 0) {
            String bodyFull = "POST body full: " + body;
            log(bodyFull.c_str());
        }
        
        bool success = updateSettingsFromJson(body);
        
        String response = "{\"success\":" + String(success ? "true" : "false") + "}";
        sendHttpResponse(client, 200, "application/json", response.c_str());
        
    } else if (path == "/api/settings/restore" && method == "POST") {
        // API endpoint to restore default settings
        log("API /api/settings/restore called");
        
        restoreDefaultSettings();
        
        String response = "{\"success\":true}";
        sendHttpResponse(client, 200, "application/json", response.c_str());
        
    } else if (path == "/api/motor" && method == "POST") {
        // API endpoint for motor control
        log("API /api/motor called");
        
        // Read POST body if Content-Length is specified
        String body = "";
        if (contentLength.length() > 0) {
            int bodyLength = contentLength.toInt();
            if (bodyLength > 0 && bodyLength < 2048) { // Reasonable limit
                char* buffer = new char[bodyLength + 1];
                int bytesRead = 0;
                unsigned long startTime = millis();
                
                // Read the exact number of bytes specified in Content-Length
                while (bytesRead < bodyLength && client.connected() && (millis() - startTime < 2000)) {
                    if (client.available()) {
                        buffer[bytesRead] = client.read();
                        bytesRead++;
                    } else {
                        delay(1);
                    }
                }
                
                buffer[bytesRead] = '\0';
                body = String(buffer);
                delete[] buffer;
                
                String readMsg = "Read " + String(bytesRead) + " bytes of " + String(bodyLength) + " expected";
                log(readMsg.c_str());
            }
        } else {
            // Fallback: read whatever is available
            delay(50); // Give time for data to arrive
            while (client.available()) {
                body += (char)client.read();
            }
        }
        
        String bodyMsg = "Motor control body: " + body;
        log(bodyMsg.c_str());
        
        // Simple JSON parsing for motor control
        bool motorEnabled = body.indexOf("\"enabled\":true") != -1;
        int speed = 0;
        
        // Extract speed value
        int speedIndex = body.indexOf("\"speed\":");
        if (speedIndex != -1) {
            String speedStr = body.substring(speedIndex + 8);
            int endIndex = speedStr.indexOf(',');
            if (endIndex == -1) endIndex = speedStr.indexOf('}');
            if (endIndex != -1) {
                speedStr = speedStr.substring(0, endIndex);
                speed = speedStr.toInt();
            }
        }
        
        // Integrate with actual motor control functions
        String controlMsg = "Remote motor control - Enabled: " + String(motorEnabled ? "true" : "false") + ", Speed: " + String(speed) + "%";
        log(controlMsg.c_str());
        
        if (motorEnabled && speed > 0) {
            // Enable remote control mode
            remoteControlActive = true;
            
            // Wake up motor if in standby
            if (motorState == standby) {
                wakeUp();
            }
            
            // Convert speed percentage (0-100) to motor steps (1-10)
            int targetStep = max(1, min(10, (speed * 10) / 100));
            currentMotorStep = targetStep;
            motorState = on;
            
            // Update lastActionTime to keep motor running (simulates button press)
            lastActionTime = micros();
            
            // Update LED bar to show new speed
            setBarSpeed(currentMotorStep);
            
            String speedMsg = "Remote control set motor to step " + String(currentMotorStep) + " (speed " + String(speed) + "%)";
            log(speedMsg.c_str());
            
        } else {
            // Disable remote control mode and stop motor
            remoteControlActive = false;
            motorState = off;
            lastActionTime = micros(); // Prevent immediate standby
            setBarSpeed(currentMotorStep); // Update display but keep step setting
            
            log("Remote control stopped motor");
        }
        
        String response = "{\"success\":true,\"enabled\":" + String(motorEnabled ? "true" : "false") + ",\"speed\":" + String(speed) + ",\"motorStep\":" + String(currentMotorStep) + "}";
        sendHttpResponse(client, 200, "application/json", response.c_str());
        
    } else if (path == "/api/lamp" && method == "POST") {
        // API endpoint for lamp control
        log("API /api/lamp called");
        
        // Read POST body if Content-Length is specified
        String body = "";
        if (contentLength.length() > 0) {
            int bodyLength = contentLength.toInt();
            if (bodyLength > 0 && bodyLength < 2048) { // Reasonable limit
                char* buffer = new char[bodyLength + 1];
                int bytesRead = 0;
                unsigned long startTime = millis();
                
                // Read the exact number of bytes specified in Content-Length
                while (bytesRead < bodyLength && client.connected() && (millis() - startTime < 2000)) {
                    if (client.available()) {
                        buffer[bytesRead] = client.read();
                        bytesRead++;
                    } else {
                        delay(1);
                    }
                }
                
                buffer[bytesRead] = '\0';
                body = String(buffer);
                delete[] buffer;
                
                String readMsg = "Read " + String(bytesRead) + " bytes of " + String(bodyLength) + " expected";
                log(readMsg.c_str());
            }
        } else {
            // Fallback: read whatever is available
            delay(50); // Give time for data to arrive
            while (client.available()) {
                body += (char)client.read();
            }
        }
        
        String bodyMsg = "Lamp control body: " + body;
        log(bodyMsg.c_str());
        
        // Extract level value
        int level = 0;
        int levelIndex = body.indexOf("\"level\":");
        if (levelIndex != -1) {
            String levelStr = body.substring(levelIndex + 8);
            int endIndex = levelStr.indexOf(',');
            if (endIndex == -1) endIndex = levelStr.indexOf('}');
            if (endIndex != -1) {
                levelStr = levelStr.substring(0, endIndex);
                level = levelStr.toInt();
            }
        }
        
        // Integrate with actual LED lamp functions
        String controlMsg = "Remote lamp control - Level: " + String(level);
        log(controlMsg.c_str());
        
        // Validate level range (0-4: LAMP_OFF to LAMP_MAX)
        if (level >= 0 && level <= 4) {
            LED_State = level;
            setLEDState(LED_State);
            setBarLED(LED_State);
            
            String levelMsg = "Remote control set lamp to level " + String(level);
            log(levelMsg.c_str());
        } else {
            String errorMsg = "Invalid lamp level: " + String(level) + " (valid: 0-4)";
            log(errorMsg.c_str());
            level = LED_State; // Return current level if invalid
        }
        
        String response = "{\"success\":true,\"level\":" + String(level) + ",\"actualLevel\":" + String(LED_State) + "}";
        sendHttpResponse(client, 200, "application/json", response.c_str());
        
    } else if (path == "/api/version") {
        // API endpoint for version information
        log("API /api/version called");
        
        String version = "2.0.0"; // Default version
        
        // Try to read version from file
        if (LittleFS.exists("/version.txt")) {
            File versionFile = LittleFS.open("/version.txt", "r");
            if (versionFile) {
                version = versionFile.readString();
                version.trim(); // Remove whitespace
                versionFile.close();
            }
        }
        
        String jsonVersion = "{\"version\":\"" + version + "\"}";
        sendHttpResponse(client, 200, "application/json", jsonVersion.c_str());
        
    } else if (path == "/api/delete-all-sessions" && method == "POST") {
        // API endpoint to delete all session files
        log("API /api/delete-all-sessions called");
        
        int deleteCount = 0;
        String errorMsg = "";
        bool success = true;
        
        try {
            // Get list of session files
            int count;
            String* sessions = listSessionFiles(&count);
            
            // Delete each session file
            for (int i = 0; i < count; i++) {
                String fullPath = "/datalog/" + sessions[i];
                if (LittleFS.exists(fullPath)) {
                    if (LittleFS.remove(fullPath)) {
                        deleteCount++;
                        String delMsg = "Deleted session file: " + fullPath;
                        log(delMsg.c_str());
                    } else {
                        errorMsg += "Failed to delete " + sessions[i] + "; ";
                        success = false;
                    }
                } else {
                    errorMsg += "File not found " + sessions[i] + "; ";
                }
            }
            
            // Clean up
            delete[] sessions;
            
            String resultMsg = "Deleted " + String(deleteCount) + " session files";
            log(resultMsg.c_str());
            
        } catch (...) {
            errorMsg = "Exception occurred during deletion";
            success = false;
        }
        
        String response;
        if (success && deleteCount > 0) {
            response = "{\"success\":true,\"deleted\":" + String(deleteCount) + ",\"message\":\"Successfully deleted " + String(deleteCount) + " session files\"}";
        } else if (deleteCount == 0) {
            response = "{\"success\":true,\"deleted\":0,\"message\":\"No session files found to delete\"}";
        } else {
            response = "{\"success\":false,\"deleted\":" + String(deleteCount) + ",\"error\":\"" + errorMsg + "\"}";
        }
        
        sendHttpResponse(client, 200, "application/json", response.c_str());
        
    } else if (path == "/api/beeper" && method == "POST") {
        // API endpoint for beeper settings (legacy compatibility)
        log("API /api/beeper called");
        
        // Read POST body
        String body = "";
        while (client.available()) {
            body += (char)client.read();
        }
        
        // Simple JSON parsing for {"enabled": true/false}
        bool newBeeperState = body.indexOf("\"enabled\":true") != -1;
        
        // Update beeper setting
        beeperEnabled = newBeeperState;
        saveBeeperSettings();
        
        String response = "{\"success\":true,\"enabled\":" + String(beeperEnabled ? "true" : "false") + "}";
        sendHttpResponse(client, 200, "application/json", response.c_str());
        
        String beeperMsg = "Beeper setting updated: " + String(beeperEnabled ? "enabled" : "disabled");
        log(beeperMsg.c_str());
        
    } else if (path == "/generate_204" || path == "/ncsi.txt" || 
               path == "/connecttest.txt" || path == "/redirect" || 
               path == "/hotspot-detect.html" || path.indexOf("success.txt") != -1 || 
               path.indexOf("success.html") != -1) {
        
        // Android/Windows/iOS captive portal detection
        log("Captive portal check detected");
        sendHttpResponse(client, 302, "text/html", "<html><head><meta http-equiv='refresh' content='0; URL=http://4.3.2.1/'></head><body>Redirecting...</body></html>");
    
    } else if (spiffsInitialized && LittleFS.exists(path)) {
        // Serve files from SPIFFS
        loadFromSPIFFS(client, path);
    } else if (isCaptivePortalRequest) {
        // Captive portal detection - redirect to our server
        log("Captive portal request detected");
        sendHttpResponse(client, 302, "text/html", "<html><head><meta http-equiv='refresh' content='0; URL=http://4.3.2.1/'></head><body>Redirecting...</body></html>");
    } else {
        // Default: redirect to root
        sendHttpResponse(client, 302, "text/plain", "Redirecting...");
    }
    
    // Close the connection
    client.stop();
}

// Webserver task that runs on Core 0
void webserverTask(void *pvParameters) {
    log("Webserver task started on Core 0");
    
    // Setup WiFi Access Point
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(ssid, password);
    
    // Log IP address - convert to String and then to char*
    String ipString = "IP: " + WiFi.softAPIP().toString();
    log(ipString.c_str());
    
    // Start DNS Server for captive portal - redirect all requests to our IP
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(DNS_PORT, "*", apIP);
    log("DNS Server started - redirecting all domains to captive portal");
    
    // Start server
    server.begin();
    log("HTTP server started");
    
    // Main loop for webserver task
    while (true) {
        // Process DNS requests for captive portal
        dnsServer.processNextRequest();
        
        // Check for HTTP clients
        WiFiClient client = server.available();
        if (client) {
            handleClient(client);
        }
        
        // Small delay to prevent watchdog trigger
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
} 