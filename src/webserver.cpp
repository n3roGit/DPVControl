#include "webserver.h"
#include "log.h"
#include "data_upload.h"
#include "datalog.h"  // Include datalogger headers
#include "constants.h" // For PIN definitions
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
// extern bool beeperEnabled; // Removed - using unified settings system

// External function declarations
extern void wakeUp(); // From motor.cpp
extern void setBarSpeed(int speed); // From ledBar.cpp
extern void setLEDState(int state); // From ledLamp.cpp
extern void setBarLED(int level); // From ledBar.cpp

// Task handle for the webserver task
TaskHandle_t webserverTaskHandle = NULL;

// WiFi Credentials
// const char* ssid = "DPVControl";
// const char* password = "DPVControl";

// DNS Server for captive portal
const byte DNS_PORT = 53;
IPAddress apIP(4, 3, 2, 1);
DNSServer dnsServer;
WiFiServer server(80);

// Flag to check if SPIFFS is mounted
bool spiffsInitialized = false;

/**
 * Generate JSON list of all available sessions sorted with newest first
 * Groups session splits together for better organization
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
        
        // Extract display name for session splits
        String displayName = sessions[i];
        String sessionNumber = "";
        String splitInfo = "";
        
        // Parse session filename like "session_0001.bin" or "session_0001-02.bin"
        if (displayName.startsWith("session_") && displayName.endsWith(".bin")) {
            String numberPart = displayName.substring(8); // Remove "session_"
            numberPart = numberPart.substring(0, numberPart.length() - 4); // Remove ".bin"
            
            int dashPos = numberPart.indexOf('-');
            if (dashPos != -1) {
                sessionNumber = numberPart.substring(0, dashPos);
                splitInfo = numberPart.substring(dashPos + 1);
                displayName = "Session " + sessionNumber + " (Part " + splitInfo + ")";
            } else {
                sessionNumber = numberPart;
                displayName = "Session " + sessionNumber;
            }
        }
        
        // Add current session indicator
        bool isCurrent = (sessions[i] == currentSession);
        if (isCurrent) {
            displayName += " (Current)";
        }
        
        // Create an object with filename, display name and current flag
        json += "{";
        json += "\"filename\":\"" + sessions[i] + "\",";
        json += "\"displayName\":\"" + displayName + "\",";
        json += "\"sessionNumber\":\"" + sessionNumber + "\",";
        json += "\"splitInfo\":\"" + splitInfo + "\",";
        json += "\"isCurrent\":" + String(isCurrent ? "true" : "false");
        json += "}";
    }
    json += "]";
    
    return json;
}

/**
 * Generate JSON data for a specific session with delta compression support and interpolation
 */
String generateSessionDataJson(String sessionFile) {
    // Ensure we have the full path
    String fullPath = sessionFile;
    if (!sessionFile.startsWith("/datalog/")) {
        fullPath = "/datalog/" + sessionFile;
    }
    
    if (!LittleFS.exists(fullPath)) {
        return "{\"data\":[],\"meta\":{\"error\":\"File not found\"}}";
    }
    
    File file = LittleFS.open(fullPath, "r");
    if (!file) {
        return "{\"data\":[],\"meta\":{\"error\":\"Cannot open file\"}}";
    }
    
    // Get file size and calculate total datapoints
    size_t fileSize = file.size();
    int totalDatapoints = fileSize / sizeof(LogdataRow);
    
    // Read first and last datapoint for metadata
    LogdataRow firstDataPoint, lastDataPoint;
    long realSessionStartMs = 0;
    long realSessionEndMs = 0;
    int realSessionDurationSeconds = 0;
    
    if (totalDatapoints > 0) {
        file.seek(0);
        file.read((uint8_t*)&firstDataPoint, sizeof(LogdataRow));
        realSessionStartMs = firstDataPoint.timestamp;
        
        file.seek((totalDatapoints - 1) * sizeof(LogdataRow));
        file.read((uint8_t*)&lastDataPoint, sizeof(LogdataRow));
        realSessionEndMs = lastDataPoint.timestamp;
        
        realSessionDurationSeconds = (realSessionEndMs - realSessionStartMs) / 1000;
    }
    
    // Calculate optimal number of points based on duration
    const int maxPoints = 1000; // Increased from 100 to support larger sessions (memory allows up to 1000 points)
    int targetPoints = maxPoints;
    
    if (realSessionDurationSeconds > 0) {
        // One point per 3 seconds for better resolution on longer sessions
        targetPoints = min(maxPoints, realSessionDurationSeconds / 3);
        targetPoints = max(10, targetPoints); // At least 10 points
    }
    
    // Calculate skip interval
    int skipInterval = totalDatapoints > targetPoints ? totalDatapoints / targetPoints : 1;
    
    // Create JSON document with fixed size
    JsonDocument doc;
    
    // Add metadata
    JsonObject meta = doc["meta"].to<JsonObject>();
    meta["realStartTimestamp"] = realSessionStartMs;
    meta["realEndTimestamp"] = realSessionEndMs;
    meta["realDurationSeconds"] = realSessionDurationSeconds;
    meta["totalDatapoints"] = totalDatapoints;
    meta["chartDatapoints"] = targetPoints;
    meta["skipInterval"] = skipInterval;
    
    // Add data array
    JsonArray data = doc["data"].to<JsonArray>();
    
    // Read and add datapoints
    for (int i = 0; i < totalDatapoints && data.size() < targetPoints; i += skipInterval) {
        LogdataRow row;
        file.seek(i * sizeof(LogdataRow));
        if (file.read((uint8_t*)&row, sizeof(LogdataRow)) == sizeof(LogdataRow)) {
            JsonObject point = data.add<JsonObject>();
            point["timestamp"] = row.timestamp;
            point["tempMotor"] = row.tempMotor;
            point["tempMosfet"] = row.tempMosfet;
            point["batteryVoltage"] = row.batteryVoltage;
            point["current"] = row.current;
            point["avgMotorCurrent"] = row.avgMotorCurrent;
            point["rpm"] = row.erpm;
            point["dutyCycle"] = row.dutyCycle;
            point["temperature"] = row.temperature;
            point["humidity"] = row.humidity;
            point["batteryLevel"] = row.batteryLevel;
            point["leakSensorState"] = row.leakSensorState;
            point["ledState"] = row.ledState;
            point["totalUptime"] = row.totalUptime;
        }
    }
    
    file.close();
    
    // Serialize to string
    String json;
    serializeJson(doc, json);
    
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
        
        /* Info icon styling */
        .info-icon {
            display: inline-block;
            width: 16px;
            height: 16px;
            background-color: #2196f3;
            color: white;
            border-radius: 50%;
            text-align: center;
            font-size: 12px;
            font-weight: bold;
            cursor: help;
            margin-left: 5px;
            line-height: 16px;
        }
        
        .info-icon:hover {
            background-color: #1976d2;
        }
    </style>
                <!-- Local Chart.js and JSZip for offline functionality -->
            <script>
                // Use our enhanced fallback as primary chart system
                let chartJsLoaded = false;
                let jsZipLoaded = false;
                
                // Initialize our built-in chart system
                console.log('Initializing built-in DPV Chart system');
                
                // Built-in Chart class optimized for DPV data
                window.Chart = class {
                    constructor(ctx, config) {
                        this.ctx = ctx;
                        this.config = config;
                        this.data = config.data || { labels: [], datasets: [] };
                        this.canvas = ctx.canvas;
                        this.canvas.style.backgroundColor = '#1e1e1e';
                        this.canvas.width = 800;
                        this.canvas.height = 400;
                        this.update();
                    }
                    
                    update() {
                        const ctx = this.ctx;
                        const canvas = this.canvas;
                        
                        // Clear canvas
                        ctx.clearRect(0, 0, canvas.width, canvas.height);
                        ctx.fillStyle = '#2a2a2a';
                        ctx.fillRect(0, 0, canvas.width, canvas.height);
                        
                        // Check if we have valid data
                        if (!this.data.datasets || this.data.datasets.length === 0 || !this.data.labels || this.data.labels.length === 0) {
                            ctx.fillStyle = '#888';
                            ctx.font = '16px Arial';
                            ctx.textAlign = 'center';
                            ctx.fillText('DPV Chart - No data available', canvas.width / 2, canvas.height / 2 - 20);
                            ctx.fillText('Waiting for sensor data...', canvas.width / 2, canvas.height / 2 + 20);
                            return;
                        }
                        
                        const legendWidth = 200;
                        const margin = 60;
                        const chartWidth = canvas.width - 2 * margin - legendWidth;
                        const chartHeight = canvas.height - 2 * margin;
                        
                        // Draw axes
                        ctx.strokeStyle = '#555';
                        ctx.lineWidth = 1;
                        ctx.beginPath();
                        ctx.moveTo(margin, margin);
                        ctx.lineTo(margin, canvas.height - margin);
                        ctx.lineTo(margin + chartWidth, canvas.height - margin);
                        ctx.stroke();
                        
                        // Colors for different datasets
                        const colors = [
                            '#4bc0c0', '#ff6384', '#ffce56', '#36a2eb', 
                            '#9966ff', '#ff9f40', '#c7c7c7', '#ff63ff',
                            '#63ff84', '#ffce84'
                        ];
                        
                        // Find global min/max for all visible datasets
                        let globalMin = Infinity;
                        let globalMax = -Infinity;
                        
                        this.data.datasets.forEach(dataset => {
                            if (dataset.data && dataset.data.length > 0) {
                                const values = dataset.data.map(d => typeof d === 'object' ? d.y : d);
                                const min = Math.min(...values);
                                const max = Math.max(...values);
                                if (min < globalMin) globalMin = min;
                                if (max > globalMax) globalMax = max;
                            }
                        });
                        
                        const range = globalMax - globalMin || 1;
                        
                        // Draw datasets
                        this.data.datasets.forEach((dataset, datasetIndex) => {
                            if (!dataset.data || dataset.data.length === 0) return;
                            
                            const color = colors[datasetIndex % colors.length];
                            ctx.strokeStyle = color;
                            ctx.lineWidth = 2;
                            ctx.beginPath();
                            
                            let hasValidPoint = false;
                            for (let i = 0; i < dataset.data.length; i++) {
                                const x = margin + (i / (dataset.data.length - 1)) * chartWidth;
                                const val = typeof dataset.data[i] === 'object' ? dataset.data[i].y : dataset.data[i];
                                const y = margin + chartHeight - ((val - globalMin) / range) * chartHeight;
                                
                                if (i === 0 || !hasValidPoint) {
                                    ctx.moveTo(x, y);
                                    hasValidPoint = true;
                                } else {
                                    ctx.lineTo(x, y);
                                }
                            }
                            ctx.stroke();
                        });
                        
                        // Draw legend on the right side
                        ctx.font = '11px Arial';
                        ctx.textAlign = 'left';
                        const legendX = margin + chartWidth + 20;
                        let legendY = margin + 20;
                        
                        ctx.fillStyle = '#ccc';
                        ctx.font = '12px Arial';
                        ctx.fillText('Parameters:', legendX, legendY);
                        legendY += 20;
                        
                        ctx.font = '10px Arial';
                        this.data.datasets.forEach((dataset, index) => {
                            if (dataset.label) {
                                const color = colors[index % colors.length];
                                ctx.fillStyle = color;
                                ctx.fillRect(legendX, legendY - 8, 12, 10);
                                ctx.fillStyle = '#ccc';
                                ctx.fillText(dataset.label, legendX + 16, legendY);
                                legendY += 14;
                            }
                        });
                        
                        // Draw title
                        ctx.fillStyle = '#4fc3f7';
                        ctx.font = 'bold 16px Arial';
                        ctx.textAlign = 'center';
                        ctx.fillText('DPV Sensor Data Visualization', (margin + chartWidth/2), 20);
                        
                        // Draw data point count
                        ctx.font = '11px Arial';
                        ctx.fillStyle = '#888';
                        ctx.fillText(`${this.data.labels.length} data points`, (margin + chartWidth/2), canvas.height - 10);
                        
                        // Draw Y-axis labels
                        ctx.font = '9px Arial';
                        ctx.textAlign = 'right';
                        ctx.fillStyle = '#888';
                        for (let i = 0; i <= 5; i++) {
                            const y = margin + (i / 5) * chartHeight;
                            const value = globalMax - (i / 5) * range;
                            ctx.fillText(value.toFixed(1), margin - 5, y + 3);
                        }
                        
                        // Draw X-axis labels (time)
                        ctx.textAlign = 'center';
                        if (this.data.labels.length > 0) {
                            const labelStep = Math.max(1, Math.floor(this.data.labels.length / 6));
                            for (let i = 0; i < this.data.labels.length; i += labelStep) {
                                const x = margin + (i / (this.data.labels.length - 1)) * chartWidth;
                                ctx.fillText(this.data.labels[i], x, canvas.height - margin + 15);
                            }
                        }
                    }
                    
                    destroy() {}
                };
                
                chartJsLoaded = true;
                
                // Load JSZip
                fetch('/jszip.min.js')
                    .then(response => {
                        if (!response.ok) throw new Error('JSZip not found');
                        return response.text();
                    })
                    .then(script => {
                        const scriptElement = document.createElement('script');
                        scriptElement.textContent = script;
                        document.head.appendChild(scriptElement);
                        jsZipLoaded = true;
                        console.log('JSZip loaded successfully from local file');
                    })
                    .catch(error => {
                        console.warn('JSZip not available locally, using fallback:', error);
                        // Fallback: Simple export functionality
                        window.JSZip = function() {
                            return {
                                file: function(name, content) {
                                    console.log('JSZip file (fallback):', name);
                                },
                                generateAsync: function(options) {
                                    return Promise.resolve(new Blob(['Mock ZIP content - JSZip not available'], {type: 'application/zip'}));
                                }
                            };
                        };
                        jsZipLoaded = true;
                    });
                
                // Enhanced CSV export functionality
                function exportToCSV(data, filename) {
                    if (!data || data.length === 0) {
                        alert('No data to export');
                        return;
                    }
                    
                    // Create proper CSV header with Total Uptime as primary time reference
                    let csv = "Total Uptime (s),Motor Temperature (degC),MOSFET Temperature (degC),Battery Voltage (V),Input Current (A),Motor Current (A),RPM,Duty Cycle (%),Ambient Temperature (degC),Humidity (%),Battery Level (%),Leak Sensor State,LED State\r\n";
                    
                    data.forEach(item => {
                        csv += [
                            item.totalUptime || 0,
                            item.tempMotor || 0,
                            item.tempMosfet || 0,
                            item.batteryVoltage || 0,
                            item.current || 0,
                            item.avgMotorCurrent || 0,
                            item.erpm || 0,
                            item.dutyCycle || 0,
                            item.temperature || 0,
                            item.humidity || 0,
                            item.batteryLevel || 0,
                            item.leakSensorState || 0,
                            item.ledState || 0
                        ].join(',') + "\r\n";
                    });
                    
                    const blob = new Blob([csv], { type: 'text/csv;charset=utf-8' });
                    const url = window.URL.createObjectURL(blob);
                    const a = document.createElement('a');
                    a.href = url;
                    a.download = filename;
                    document.body.appendChild(a);
                    a.click();
                    document.body.removeChild(a);
                    window.URL.revokeObjectURL(url);
                    
                    console.log('CSV exported: ' + filename + ' with ' + data.length + ' data points');
                }
            </script>
</head>
<body>
    <div class="container">
        <h1>DPVControl Web Interface</h1>
        
        <div class="section">
            <div class="tab-navigation">
                <button class="nav-tab active" onclick="showTab('status')">Status</button>
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
                        <td>Data Points:</td>
                        <td class="status-value" id="dataPointCount">Loading...</td>
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
        

        
        <div id="charts-tab" class="tab-content">
            <div class="section">
                <h2>Data Visualization</h2>
                
                <!-- Chart Controls -->
                <div style="margin-bottom: 20px; padding: 15px; background-color: #3a3a3a; border-radius: 8px; border: 1px solid #505050;">
                    <div style="display: flex; flex-wrap: wrap; gap: 15px; align-items: center;">
                        <div>
                            <label for="sessionSelect" style="color: #b0b0b0; font-weight: bold; margin-right: 8px;">Session:</label>
                            <select id="sessionSelect" onchange="updateSessionFilter()" style="padding: 6px; border: 1px solid #555; border-radius: 4px; background-color: #4a4a4a; color: #e0e0e0;">
                                <!-- Sessions will be populated dynamically -->
                            </select>
                        </div>
                        
                        <div id="sessionDurationInfo" style="color: #4fc3f7; font-weight: bold; font-size: 14px;">
                            <!-- Session duration will be displayed here -->
                        </div>
                        
                        <div id="timeSliderContainer" style="flex: 1; min-width: 200px;">
                            <label for="timeSlider" style="color: #b0b0b0; font-weight: bold; display: block; margin-bottom: 5px;">
                                Time Window Position:
                            </label>
                            <input type="range" id="timeSlider" min="0" max="100" value="100" 
                                   style="width: 100%;" onchange="updateTimeWindow()" oninput="updateTimeWindow()">
                            <div style="display: flex; justify-content: space-between; font-size: 12px; color: #888; margin-top: 3px;">
                                <span id="sliderStart">Oldest</span>
                                <span id="sliderEnd">Newest</span>
                            </div>
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
                    <button class="button" onclick="exportAllSessionsAsZip()" style="background-color: #e67e22;">
                        Export All Sessions as CSV
                    </button>
                </div>
                
                <p style="margin-top: 10px; font-size: 12px; color: #888;">
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
                    
                    <!-- ON/OFF Toggle -->
                    <div style="text-align: center; margin-bottom: 20px;">
                        <button id="lampToggleBtn" class="button" onclick="toggleLamp()" 
                                style="background-color: #666; font-size: 18px; padding: 15px 30px;">
                            💡 LAMP OFF
                        </button>
                    </div>
                    
                    <!-- Brightness Level Selection -->
                    <div id="lampLevelsContainer" style="margin-bottom: 20px; opacity: 0.5;">
                        <label style="display: block; margin-bottom: 15px; font-weight: bold; text-align: center;">
                            Brightness Level:
                        </label>
                        <div id="lampLevelRadios" style="display: flex; flex-direction: column; gap: 10px;">
                            <!-- Lamp level radio buttons will be generated dynamically by JavaScript -->
                        </div>
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
                    <button class="button" onclick="rebootSystem()" style="background-color: #ff5722; margin-left: 10px;">🔄 Reboot System</button>
                    <br>
                    <button class="button" onclick="exportSettings()" style="background-color: #ff9800; margin-top: 10px;">Export Settings</button>
                    <button class="button" onclick="importSettings()" style="background-color: #9c27b0; margin-top: 10px;">Import Settings</button>
                    <input type="file" id="settingsFileInput" accept=".json" style="display: none;" onchange="handleSettingsFile(event)">
                </div>
                
                <!-- Settings Form -->
                <form id="settingsForm">
                    <!-- Motor and Speed Settings -->
                    <div class="settings-group">
                        <h3>Motor and Speed Settings</h3>
                        <table class="settings-table">
                            <tr>
                                <td><label for="speedSteps">Speed Steps:</label><span class="info-icon" title="Number of speed levels available (2-20). More steps = finer speed control.">?</span></td>
                                <td><input type="number" id="speedSteps" min="2" max="20" value="10"></td>
                            </tr>
                            <tr>
                                <td><label for="standbyDelaySeconds">Standby Delay (s):</label><span class="info-icon" title="Time in seconds before the system enters standby mode when inactive.">?</span></td>
                                <td><input type="number" id="standbyDelaySeconds" min="10" max="3600" value="300"></td>
                            </tr>
                            <tr>
                                <td><label for="batteryPowerMax">Battery Power Max (W):</label><span class="info-icon" title="Maximum power consumption from battery in watts. Used for power calculations and safety limits.">?</span></td>
                                <td><input type="number" id="batteryPowerMax" min="100" max="5000" value="1000"></td>
                            </tr>
                            <tr>
                                <td><label for="minSpeedPercent">Min Speed Percent:</label><span class="info-icon" title="Minimum speed as percentage (0.01-1.0). Lower values allow slower minimum speeds.">?</span></td>
                                <td><input type="number" id="minSpeedPercent" min="0.01" max="1.0" step="0.01" value="0.38"></td>
                            </tr>
                            <tr>
                                <td><label for="maxSpeedRpm">Max Speed eRPM:</label><span class="info-icon" title="Maximum electrical RPM of the motor. Higher values = higher top speed. Typical range: 3000-20000 eRPM.">?</span></td>
                                <td><input type="number" id="maxSpeedRpm" min="1000" max="50000" value="15800"></td>
                            </tr>
                            <tr>
                                <td><label for="speedUpTimeMs">Speed Up Time (ms):</label><span class="info-icon" title="Time in milliseconds for motor to accelerate between speed steps. Higher = smoother acceleration.">?</span></td>
                                <td><input type="number" id="speedUpTimeMs" min="100" max="5000" value="1000"></td>
                            </tr>
                            <tr>
                                <td><label for="speedDownTimeMs">Speed Down Time (ms):</label><span class="info-icon" title="Time in milliseconds for motor to decelerate between speed steps. Higher = smoother deceleration.">?</span></td>
                                <td><input type="number" id="speedDownTimeMs" min="100" max="5000" value="1000"></td>
                            </tr>
                            <tr>
                                <td><label for="maxTimeOverloadedMs">Max Time Overloaded (ms):</label><span class="info-icon" title="Maximum time motor can be overloaded before protection kicks in. Safety feature to prevent damage.">?</span></td>
                                <td><input type="number" id="maxTimeOverloadedMs" min="1000" max="30000" value="5000"></td>
                            </tr>
                        </table>
                    </div>
                    
                    <!-- Jam Detection Settings -->
                    <div class="settings-group">
                        <h3>Jam Detection Settings</h3>
                        <table class="settings-table">
                            <tr>
                                <td><label for="jamMin">Jam Min:</label><span class="info-icon" title="Minimum current threshold for jam detection. Lower values = more sensitive jam detection.">?</span></td>
                                <td><input type="number" id="jamMin" min="0.1" max="10.0" step="0.1" value="0.5"></td>
                            </tr>
                            <tr>
                                <td><label for="jamDetectionThreshold">Jam Detection Threshold:</label><span class="info-icon" title="Current threshold above which jam protection activates. Higher values = less sensitive.">?</span></td>
                                <td><input type="number" id="jamDetectionThreshold" min="0.5" max="20.0" step="0.1" value="2.0"></td>
                            </tr>
                        </table>
                    </div>
                    
                    <!-- Battery Settings -->
                    <div class="settings-group">
                        <h3>Battery Settings</h3>
                        <table class="settings-table">
                            <tr>
                                <td><label for="cellsInSeries">Cells in Series:</label><span class="info-icon" title="Number of battery cells connected in series. Used for voltage calculations and battery level estimation.">?</span></td>
                                <td><input type="number" id="cellsInSeries" min="1" max="20" value="6"></td>
                            </tr>
                        </table>
                    </div>
                    
                    <!-- LED Bar Settings -->
                    <div class="settings-group">
                        <h3>LED Bar Settings</h3>
                        <table class="settings-table">
                            <tr>
                                <td><label for="ledBarNum">LED Bar Number:</label><span class="info-icon" title="Number of LEDs in the status bar (1-50). More LEDs = finer status display resolution.">?</span></td>
                                <td><input type="number" id="ledBarNum" min="1" max="50" value="10"></td>
                            </tr>
                            <tr>
                                <td><label for="ledBarBrightness">LED Bar Brightness:</label><span class="info-icon" title="Main brightness level for LED bar (1-255). Higher values = brighter display.">?</span></td>
                                <td><input type="number" id="ledBarBrightness" min="1" max="255" value="15"></td>
                            </tr>
                            <tr>
                                <td><label for="ledBarBrightnessSecond">LED Bar Brightness Second:</label><span class="info-icon" title="Secondary brightness level for standby/dimmed mode (1-255). Used for low-power indication.">?</span></td>
                                <td><input type="number" id="ledBarBrightnessSecond" min="1" max="255" value="3"></td>
                            </tr>
                            <tr>
                                <td><label for="ledFrequency">LED Frequency:</label><span class="info-icon" title="PWM frequency for LED control in Hz (100-10000). Higher frequencies reduce flicker.">?</span></td>
                                <td><input type="number" id="ledFrequency" min="100" max="10000" value="960"></td>
                            </tr>
                        </table>
                    </div>
                    
                    <!-- Lamp Settings -->
                    <div class="settings-group">
                        <h3>Front Lamp Settings</h3>
                        <table class="settings-table">
                            <tr>
                                <td><label for="lampMaxLevels">Number of Lamp Levels:</label><span class="info-icon" title="Number of brightness levels for front lamp (2-10). More levels = finer brightness control.">?</span></td>
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
                                <td><label for="wifiSSID">WiFi SSID:</label><span class="info-icon" title="WiFi network name for the DPV access point. Change to customize your network name.">?</span></td>
                                <td><input type="text" id="wifiSSID" maxlength="31" value="DPVControl"></td>
                            </tr>
                            <tr>
                                <td><label for="wifiPassword">WiFi Password:</label><span class="info-icon" title="WiFi password for the DPV access point. Minimum 8 characters for security.">?</span></td>
                                <td><input type="password" id="wifiPassword" maxlength="31" value="DPVControl"></td>
                            </tr>
                        </table>
                    </div>
                    
                    <!-- System Settings -->
                    <div class="settings-group">
                        <h3>System Settings</h3>
                        <table class="settings-table">
                            <tr>
                                <td><label for="beeperEnabled">Beeper Enabled:</label><span class="info-icon" title="Enable/disable audio feedback beeps for system status and warnings.">?</span></td>
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
                    <div style="background: linear-gradient(45deg, #2196f3, #4fc3f7); 
                                color: white; 
                                padding: 20px; 
                                border-radius: 10px; 
                                text-align: center; 
                                font-size: 24px; 
                                font-weight: bold; 
                                margin-bottom: 15px;
                                max-width: 200px;
                                margin: 0 auto 15px auto;">
                        <img

          alt="DPV-Bild"
          style="max-width:100%; height:auto;"
        >
                    </div>
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
        let sessionMetadata = null; // Store session metadata (real duration, total datapoints, etc.)
        let timeSliderValue = 100;
        let systemStartTime = null;
        let availableSessions = [];
        let selectedSession = null; // No "all sessions" option
        let currentTimeRange = 'recent'; // Default time range
        const FIXED_TIME_RANGE_MINUTES = 5; // Fixed 5-minute window
        const FIXED_UPDATE_INTERVAL_MS = 10000; // Fixed 10-second updates
        
        // Initialize the application
        document.addEventListener('DOMContentLoaded', function() {
            // Wait for Chart.js and JSZip to load before initializing
            function waitForLibraries() {
                if (typeof Chart !== 'undefined' && typeof JSZip !== 'undefined') {
                    console.log('All libraries loaded, initializing application');
                    
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
                    
                    // Enable remote control interface after initialization
                    setTimeout(enableRemoteControlInterface, 1000);
                } else {
                    console.log('Waiting for libraries to load... Chart:', typeof Chart, 'JSZip:', typeof JSZip);
                    setTimeout(waitForLibraries, 200);
                }
            }
            
            // Start waiting for libraries
            setTimeout(waitForLibraries, 500); // Give initial load time
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
                // Use displayName if available, otherwise fallback to old format
                option.textContent = session.displayName || session.filename.replace('session_', 'Session ').replace('.bin', '');
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
            console.log('Time slider moved to:', timeSliderValue, '%');
            
            // Directly update the chart without reloading data
            if (allDataPoints && allDataPoints.length > 0) {
                updateCharts(allDataPoints);
            }
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
        
        // Calculate and display session duration
        function updateSessionDurationInfo(data) {
            const durationInfo = document.getElementById('sessionDurationInfo');
            if (!durationInfo) return;
            
            // Use real session metadata when available (preferred)
            if (sessionMetadata && sessionMetadata.realDurationSeconds !== undefined) {
                const realDurationSeconds = sessionMetadata.realDurationSeconds;
                const totalDatapoints = sessionMetadata.totalDatapoints || data.length;
                const chartDatapoints = sessionMetadata.chartDatapoints || data.length;
                
                // Format duration as minutes and seconds
                const minutes = Math.floor(realDurationSeconds / 60);
                const seconds = realDurationSeconds % 60;
                
                // Get session name for display
                let sessionName = 'Current Session';
                if (selectedSession) {
                    const sessionNumber = selectedSession.replace('session_', '').replace('.bin', '');
                    sessionName = `Session ${sessionNumber}`;
                    
                    // Check if it's current session
                    const currentSession = availableSessions.find(s => s.filename === selectedSession && s.isCurrent);
                    if (currentSession) {
                        sessionName += ' (Current)';
                    }
                }
                
                // Format the display text with real duration
                let durationText = '';
                if (minutes > 0) {
                    durationText = `${minutes} Min. ${seconds} Sec. (${totalDatapoints} data points, ${chartDatapoints} displayed)`;
                } else {
                    durationText = `${seconds} Sec. (${totalDatapoints} data points, ${chartDatapoints} displayed)`;
                }
                
                durationInfo.textContent = durationText;
                console.log('Using REAL session duration:', realDurationSeconds + 's from metadata');
                return;
            }
            
            // Fallback: Calculate from chart data (old method) 
            if (!data || data.length === 0) {
                durationInfo.textContent = '';
                return;
            }
            
            console.log('Warning: Using chart data for duration calculation (may be inaccurate due to data reduction)');
            
            // Calculate session duration from first to last timestamp in chart data
            const startTime = new Date(data[0].timestamp);
            const endTime = new Date(data[data.length - 1].timestamp);
            const durationMs = endTime - startTime;
            const durationSeconds = Math.floor(durationMs / 1000);
            
            // Format duration as minutes and seconds
            const minutes = Math.floor(durationSeconds / 60);
            const seconds = durationSeconds % 60;
            
            // Get session name for display
            let sessionName = 'Current Session';
            if (selectedSession) {
                const sessionNumber = selectedSession.replace('session_', '').replace('.bin', '');
                sessionName = `Session ${sessionNumber}`;
                
                // Check if it's current session
                const currentSession = availableSessions.find(s => s.filename === selectedSession && s.isCurrent);
                if (currentSession) {
                    sessionName += ' (Current)';
                }
            }
            
            // Format the display text
            let durationText = '';
            if (minutes > 0) {
                durationText = `~${minutes} Min. ${seconds} Sec. (${data.length} data points, possibly reduced)`;
            } else {
                durationText = `~${seconds} Sec. (${data.length} data points, possibly reduced)`;
            }
            
            durationInfo.textContent = durationText;
        }
        
        // Filter data based on time window slider position
        function filterDataByTimeRange(data) {
            // Use the new display window function with increased limits
            return getDisplayWindow(data, timeSliderValue);
        }
        
        // Time window management for chart display
        function getDisplayWindow(data, timeSliderValue) {
            if (!data || data.length === 0) return [];
            
            // Increased display window for better data visibility
            const FIXED_WINDOW_POINTS = 500; // Increased from 60 for better session coverage
            if (data.length <= FIXED_WINDOW_POINTS) {
                // Show all data if session is small enough
                const slider = document.getElementById('timeSlider');
                if (slider) slider.style.opacity = '0.3';
                return data;
            } else {
                // For larger sessions, enable time slider
                const slider = document.getElementById('timeSlider');
                if (slider) slider.style.opacity = '1.0';
            }
            
            // Calculate window position based on slider (0 = oldest, 100 = newest)
            const windowSize = FIXED_WINDOW_POINTS;
            const maxStartIndex = data.length - windowSize;
            const startIndex = Math.floor((maxStartIndex * timeSliderValue) / 100);
            const endIndex = Math.min(startIndex + windowSize, data.length);
            
            return data.slice(startIndex, endIndex);
        }
        
        // Update slider labels with proper time formatting
        function updateSliderLabels(data) {
            if (!data || data.length === 0) return;
            
            const sliderStart = document.getElementById('sliderStart');
            const sliderEnd = document.getElementById('sliderEnd');
            
            if (!sliderStart || !sliderEnd) return;
            
            // Use first timestamp as session start reference
            const sessionStart = data[0].timestamp;
            
            // For small sessions, show relative times from start and disable slider
            if (data.length <= 500) { // Updated threshold from 60 to 500
                sliderStart.textContent = formatTimeOnly(data[0].timestamp, sessionStart);
                sliderEnd.textContent = formatTimeOnly(data[data.length - 1].timestamp, sessionStart);
                return;
            }
            
            // For long sessions, calculate current window based on slider position
            const windowSize = 500; // Updated from 60 to 500
            const maxStartIndex = data.length - windowSize;
            const currentStartIndex = Math.floor((maxStartIndex * timeSliderValue) / 100);
            const currentEndIndex = Math.min(currentStartIndex + windowSize, data.length);
            
            sliderStart.textContent = formatTimeOnly(data[currentStartIndex].timestamp, sessionStart);
            sliderEnd.textContent = formatTimeOnly(data[currentEndIndex - 1].timestamp, sessionStart);
        }
        
        // Format time as MM:SS relative to session start
        function formatTimeOnly(timestamp, sessionStartTimestamp) {
            // If we have a session start reference, show relative time
            if (sessionStartTimestamp) {
                const relativeMs = timestamp - sessionStartTimestamp;
                const totalSeconds = Math.floor(relativeMs / 1000);
                const minutes = Math.floor(totalSeconds / 60);
                const seconds = totalSeconds % 60;
                
                return String(minutes).padStart(2, '0') + ':' + String(seconds).padStart(2, '0');
            }
            
            // Fallback: try to parse as date
            const date = new Date(timestamp);
            if (isNaN(date.getTime())) {
                // If timestamp is not a valid date, treat as relative milliseconds
                const totalSeconds = Math.floor(timestamp / 1000);
                const minutes = Math.floor(totalSeconds / 60);
                const seconds = totalSeconds % 60;
                
                return String(minutes).padStart(2, '0') + ':' + String(seconds).padStart(2, '0');
            }
            
            return date.toLocaleTimeString('de-DE', {
                hour: '2-digit',
                minute: '2-digit',
                second: '2-digit'
            });
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
                    
                    // Safe element updates with null checks
                    const uptime = document.getElementById('uptime');
                    if (uptime) uptime.textContent = formatTime(data.uptime || 0);
                    
                    const totalUptime = document.getElementById('totalUptime');
                    if (totalUptime) totalUptime.textContent = formatTime((data.totalUptime || 0) * 1000);
                    
                    const dataPointCount = document.getElementById('dataPointCount');
                    if (dataPointCount) dataPointCount.textContent = data.dataPoints || 0;
                    
                    // Update additional sensor data with null checks
                    const waterSensorFront = document.getElementById('waterSensorFront');
                    if (waterSensorFront) waterSensorFront.textContent = data.waterSensorFront === true ? 'LEAK DETECTED!' : 'OK';
                    
                    const waterSensorBack = document.getElementById('waterSensorBack');
                    if (waterSensorBack) waterSensorBack.textContent = data.waterSensorBack === true ? 'LEAK DETECTED!' : 'OK';
                    
                    const leftButton = document.getElementById('leftButton');
                    if (leftButton) leftButton.textContent = data.leftButton === true ? 'PRESSED' : 'RELEASED';
                    
                    const rightButton = document.getElementById('rightButton');
                    if (rightButton) rightButton.textContent = data.rightButton === true ? 'PRESSED' : 'RELEASED';
                    
                    const lampLevel = document.getElementById('lampLevel');
                    if (lampLevel) lampLevel.textContent = 'Level ' + (data.lampLevel || 0);
                    
                    const beeperStatus = document.getElementById('beeperStatus');
                    if (beeperStatus) beeperStatus.textContent = data.beeperEnabled === true ? 'Enabled' : 'Disabled';
                    
                    // Update beeper setting checkbox (new settings tab)
                    const beeperEnabled = document.getElementById('beeperEnabled');
                    if (beeperEnabled) beeperEnabled.checked = data.beeperEnabled === true;
                })
                .catch(error => {
                    console.error('Error fetching status:', error);
                    const uptime = document.getElementById('uptime');
                    if (uptime) uptime.textContent = 'Error loading';
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
                        
                        // Safe element updates with null checks
                        const battery = document.getElementById('battery');
                        if (battery) battery.textContent = (latest.batteryVoltage || 0).toFixed(2) + ' V';
                        
                        const motorTemp = document.getElementById('motorTemp');
                        if (motorTemp) motorTemp.textContent = (latest.tempMotor || 0).toFixed(1) + ' °C';
                        
                        const mosfetTemp = document.getElementById('mosfetTemp');
                        if (mosfetTemp) mosfetTemp.textContent = (latest.tempMosfet || 0).toFixed(1) + ' °C';
                        
                        const temperature = document.getElementById('temperature');
                        if (temperature) temperature.textContent = (latest.temperature || 0).toFixed(1) + ' °C';
                        
                        const humidity = document.getElementById('humidity');
                        if (humidity) humidity.textContent = (latest.humidity || 0).toFixed(1) + ' %';
                        
                        const current = document.getElementById('current');
                        if (current) current.textContent = (latest.current || 0).toFixed(2) + ' A';
                        
                        const motorCurrent = document.getElementById('motorCurrent');
                        if (motorCurrent) motorCurrent.textContent = (latest.avgMotorCurrent || 0).toFixed(2) + ' A';
                        
                        const rpm = document.getElementById('rpm');
                        if (rpm) rpm.textContent = (latest.erpm || 0).toFixed(0) + ' eRPM';
                        
                        const dutyCycle = document.getElementById('dutyCycle');
                        if (dutyCycle) dutyCycle.textContent = (latest.dutyCycle || 0).toFixed(1) + ' %';
                        
                        const batteryLevel = document.getElementById('batteryLevel');
                        if (batteryLevel) batteryLevel.textContent = (latest.batteryLevel || 0) + ' %';
                        
                        const waterSensorFront = document.getElementById('waterSensorFront');
                        if (waterSensorFront) waterSensorFront.textContent = latest.leakSensorState === 1 ? 'LEAK DETECTED!' : 'OK';
                    }
                })
                .catch(error => {
                    console.error('Error fetching data:', error);
                    const battery = document.getElementById('battery');
                    if (battery) battery.textContent = 'Error loading';
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
                    const currentVersion = document.getElementById('currentVersion');
                    if (currentVersion) currentVersion.textContent = data.version || 'Unknown';
                })
                .catch(error => {
                    console.error('Error fetching version:', error);
                    const currentVersion = document.getElementById('currentVersion');
                    if (currentVersion) currentVersion.textContent = 'Unknown';
                });
            
            // Update system uptime and total runtime from status data
            fetch('/api/status')
                .then(response => response.json())
                .then(data => {
                    const systemUptime = document.getElementById('systemUptime');
                    if (systemUptime) systemUptime.textContent = formatTime(data.uptime || 0);
                    
                    const totalRuntime = document.getElementById('totalRuntime');
                    if (totalRuntime) totalRuntime.textContent = formatTime((data.totalUptime || 0) * 1000);
                })
                .catch(error => {
                    console.error('Error fetching uptime:', error);
                    const systemUptime = document.getElementById('systemUptime');
                    if (systemUptime) systemUptime.textContent = 'Unknown';
                    
                    const totalRuntime = document.getElementById('totalRuntime');
                    if (totalRuntime) totalRuntime.textContent = 'Unknown';
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
                .then(jsonResponse => {
                    // Handle new JSON format with metadata and data
                    if (jsonResponse.data && jsonResponse.meta) {
                        // New format with metadata
                        console.log('Chart data received:', jsonResponse.data.length, 'points, real duration:', jsonResponse.meta.realDurationSeconds + 's');
                        console.log('Session metadata:', jsonResponse.meta);
                        
                        allDataPoints = jsonResponse.data;
                        sessionMetadata = jsonResponse.meta; // Store metadata globally
                        updateCharts(jsonResponse.data);
                    } else if (Array.isArray(jsonResponse)) {
                        // Legacy format (array only)
                        console.log('Chart data received (legacy format):', jsonResponse.length, 'points');
                        allDataPoints = jsonResponse;
                        sessionMetadata = null; // No metadata available
                        updateCharts(jsonResponse);
                    } else {
                        throw new Error('Invalid response format');
                    }
                })
                .catch(error => {
                    console.error('Error fetching chart data:', error);
                    // Show empty chart on error
                    allDataPoints = [];
                    sessionMetadata = null;
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
                // Clear session duration info
                const durationInfo = document.getElementById('sessionDurationInfo');
                if (durationInfo) durationInfo.textContent = '';
                return;
            }
            
            // Calculate and display session duration
            updateSessionDurationInfo(data);
            
            // Filter data based on time range and slider
            const filteredData = filterDataByTimeRange(data);
            console.log('Filtered data:', filteredData.length, 'points');
            
            // Prepare labels (timestamps) using relative time from session start
            const sessionStart = allDataPoints.length > 0 ? allDataPoints[0].timestamp : filteredData[0].timestamp;
            const labels = filteredData.map(item => {
                return formatTimeOnly(item.timestamp, sessionStart);
            });
            console.log('Generated', labels.length, 'relative time labels');
            
            // Update Combined Chart
            charts.combinedChart.data.labels = labels;
            charts.combinedChart.data.datasets[0].data = filteredData.map(item => item.batteryVoltage);
            charts.combinedChart.data.datasets[1].data = filteredData.map(item => item.current);
            charts.combinedChart.data.datasets[2].data = filteredData.map(item => item.tempMotor);
            charts.combinedChart.data.datasets[3].data = filteredData.map(item => item.temperature);
            charts.combinedChart.data.datasets[4].data = filteredData.map(item => item.humidity);
            charts.combinedChart.data.datasets[5].data = filteredData.map(item => item.erpm / 100);
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
                html += '<td style="border: 1px solid #ddd; padding: 8px;">' + item.erpm.toFixed(0) + '</td>';
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
            
            // Update remote control lamp slider after changing levels
            updateRemoteLampSlider();
            
            // Update remote control lamp level interface with new brightness values
            updateLampLevelInterface();
        }
        
        // Update remote control lamp slider based on current settings
        function updateRemoteLampSlider() {
            const lampSlider = document.getElementById('lampLevelSlider');
            if (!lampSlider) return;
            
            // Get current lamp settings
            const maxLevels = parseInt(document.getElementById('lampMaxLevels')?.value || 5);
            
            // Update slider to work with percentage (0-100%)
            lampSlider.min = 0;
            lampSlider.max = 100;
            lampSlider.step = Math.round(100 / maxLevels); // Dynamic step based on levels
            
            console.log('Updated lamp slider: maxLevels=' + maxLevels + ', step=' + lampSlider.step + '%');
        }
        
        // Load DPV settings from API
        function loadDPVSettings() {
            console.log('loadDPVSettings() called');
            document.getElementById('settingsStatus').textContent = 'Loading settings...';
            
            fetch('/api/settings')
                .then(response => {
                    console.log('Settings API response status:', response.status);
                    if (!response.ok) {
                        throw new Error('Network response was not ok: ' + response.status);
                    }
                    return response.json();
                })
                .then(data => {
                    console.log('Settings data received:', data);
                    // Motor and speed settings
                    const speedStepsEl = document.getElementById('speedSteps');
                    if (speedStepsEl) speedStepsEl.value = data.speedSteps || 10;
                    
                    const standbyDelayEl = document.getElementById('standbyDelaySeconds');
                    if (standbyDelayEl) standbyDelayEl.value = data.standbyDelaySeconds || 300;
                    
                    const batteryPowerMaxEl = document.getElementById('batteryPowerMax');
                    if (batteryPowerMaxEl) batteryPowerMaxEl.value = data.batteryPowerMax || 1000;
                    
                    const minSpeedPercentEl = document.getElementById('minSpeedPercent');
                    if (minSpeedPercentEl) minSpeedPercentEl.value = parseFloat(data.minSpeedPercent || 0.1).toFixed(2);
                    
                    const maxSpeedRpmEl = document.getElementById('maxSpeedRpm');
                    if (maxSpeedRpmEl) maxSpeedRpmEl.value = data.maxSpeedRpm || 3000;
                    
                    const speedUpTimeEl = document.getElementById('speedUpTimeMs');
                    if (speedUpTimeEl) speedUpTimeEl.value = data.speedUpTimeMs || 1000;
                    
                    const speedDownTimeEl = document.getElementById('speedDownTimeMs');
                    if (speedDownTimeEl) speedDownTimeEl.value = data.speedDownTimeMs || 1000;
                    
                    const maxTimeOverloadedEl = document.getElementById('maxTimeOverloadedMs');
                    if (maxTimeOverloadedEl) maxTimeOverloadedEl.value = data.maxTimeOverloadedMs || 5000;
                    
                    // Jam detection
                    const jamMinEl = document.getElementById('jamMin');
                    if (jamMinEl) jamMinEl.value = parseFloat(data.jamMin || 0.5).toFixed(2);
                    
                    const jamDetectionEl = document.getElementById('jamDetectionThreshold');
                    if (jamDetectionEl) jamDetectionEl.value = parseFloat(data.jamDetectionThreshold || 2.0).toFixed(2);
                    
                    // Battery
                    const cellsInSeriesEl = document.getElementById('cellsInSeries');
                    if (cellsInSeriesEl) cellsInSeriesEl.value = data.cellsInSeries || 6;
                    
                    // LED Bar
                    const ledBarNumEl = document.getElementById('ledBarNum');
                    if (ledBarNumEl) ledBarNumEl.value = data.ledBarNum || 10;
                    
                    const ledBarBrightnessEl = document.getElementById('ledBarBrightness');
                    if (ledBarBrightnessEl) ledBarBrightnessEl.value = data.ledBarBrightness || 50;
                    
                    const ledBarBrightnessSecondEl = document.getElementById('ledBarBrightnessSecond');
                    if (ledBarBrightnessSecondEl) ledBarBrightnessSecondEl.value = data.ledBarBrightnessSecond || 25;
                    
                    const ledFrequencyEl = document.getElementById('ledFrequency');
                    if (ledFrequencyEl) ledFrequencyEl.value = data.ledFrequency || 800;
                    
                    // Lamp settings
                    const lampMaxLevelsEl = document.getElementById('lampMaxLevels');
                    if (lampMaxLevelsEl) {
                        lampMaxLevelsEl.value = data.lampMaxLevels || 5;
                        updateLampBrightnessInputs();
                        for (let i = 0; i < (data.lampMaxLevels || 5); i++) {
                            const lampBrightnessEl = document.getElementById('lampBrightness' + i);
                            if (lampBrightnessEl && data.lampBrightness && data.lampBrightness[i] !== undefined) {
                                lampBrightnessEl.value = data.lampBrightness[i];
                            }
                        }
                    }
                    
                    // WiFi
                    const wifiSSIDEl = document.getElementById('wifiSSID');
                    if (wifiSSIDEl) wifiSSIDEl.value = data.wifiSSID || '';
                    
                    const wifiPasswordEl = document.getElementById('wifiPassword');
                    if (wifiPasswordEl) wifiPasswordEl.value = data.wifiPassword || '';
                    
                    // System
                    const beeperEnabledEl = document.getElementById('beeperEnabled');
                    if (beeperEnabledEl) beeperEnabledEl.checked = data.beeperEnabled || false;
                    
                    const debugLoggingEnabledEl = document.getElementById('debugLoggingEnabled');
                    if (debugLoggingEnabledEl) debugLoggingEnabledEl.checked = data.debugLoggingEnabled || false;
                    
                    const standbyBlinkStartEl = document.getElementById('standbyBlinkStartMinutes');
                    if (standbyBlinkStartEl) standbyBlinkStartEl.value = data.standbyBlinkStartMinutes || 5;
                    
                    const standbyBlinkDurationEl = document.getElementById('standbyBlinkDurationSeconds');
                    if (standbyBlinkDurationEl) standbyBlinkDurationEl.value = data.standbyBlinkDurationSeconds || 10;
                    
                    const settingsStatusEl = document.getElementById('settingsStatus');
                    if (settingsStatusEl) settingsStatusEl.textContent = 'Settings loaded successfully';
                    console.log('Settings loaded and applied to form successfully');
                    
                    // Update remote control lamp slider interface after all settings are loaded
                    updateLampSliderInterface();
                    
                    // Update remote control lamp level interface after all settings are loaded
                    updateLampLevelInterface();
                })
                .catch(error => {
                    console.error('Error loading settings:', error);
                    document.getElementById('settingsStatus').textContent = 'Error loading settings: ' + error.message;
                });
        }
        
        // Save DPV settings to API
        function saveDPVSettings() {
            console.log('saveDPVSettings() called');
            const settingsStatusEl = document.getElementById('settingsStatus');
            if (settingsStatusEl) settingsStatusEl.textContent = 'Saving settings...';
            
            // Collect lamp brightness values with null checks
            const lampMaxLevelsEl = document.getElementById('lampMaxLevels');
            const maxLevels = lampMaxLevelsEl ? parseInt(lampMaxLevelsEl.value) || 5 : 5;
            const lampBrightness = [];
            for (let i = 0; i < 10; i++) {
                if (i < maxLevels) {
                    const lampBrightnessEl = document.getElementById('lampBrightness' + i);
                    lampBrightness[i] = lampBrightnessEl ? parseInt(lampBrightnessEl.value) || 0 : 0;
                } else {
                    lampBrightness[i] = 0;
                }
            }
            
            // Collect all settings with null checks and fallback values
            const speedStepsEl = document.getElementById('speedSteps');
            const standbyDelayEl = document.getElementById('standbyDelaySeconds');
            const batteryPowerMaxEl = document.getElementById('batteryPowerMax');
            const minSpeedPercentEl = document.getElementById('minSpeedPercent');
            const maxSpeedRpmEl = document.getElementById('maxSpeedRpm');
            const speedUpTimeEl = document.getElementById('speedUpTimeMs');
            const speedDownTimeEl = document.getElementById('speedDownTimeMs');
            const maxTimeOverloadedEl = document.getElementById('maxTimeOverloadedMs');
            const jamMinEl = document.getElementById('jamMin');
            const jamDetectionEl = document.getElementById('jamDetectionThreshold');
            const cellsInSeriesEl = document.getElementById('cellsInSeries');
            const ledBarNumEl = document.getElementById('ledBarNum');
            const ledBarBrightnessEl = document.getElementById('ledBarBrightness');
            const ledBarBrightnessSecondEl = document.getElementById('ledBarBrightnessSecond');
            const ledFrequencyEl = document.getElementById('ledFrequency');
            const wifiSSIDEl = document.getElementById('wifiSSID');
            const wifiPasswordEl = document.getElementById('wifiPassword');
            const beeperEnabledEl = document.getElementById('beeperEnabled');
            const debugLoggingEnabledEl = document.getElementById('debugLoggingEnabled');
            const standbyBlinkStartEl = document.getElementById('standbyBlinkStartMinutes');
            const standbyBlinkDurationEl = document.getElementById('standbyBlinkDurationSeconds');
            
            const settingsData = {
                speedSteps: speedStepsEl ? parseInt(speedStepsEl.value) || 10 : 10,
                standbyDelaySeconds: standbyDelayEl ? parseInt(standbyDelayEl.value) || 300 : 300,
                batteryPowerMax: batteryPowerMaxEl ? parseInt(batteryPowerMaxEl.value) || 1000 : 1000,
                minSpeedPercent: minSpeedPercentEl ? parseFloat(minSpeedPercentEl.value) || 0.1 : 0.1,
                maxSpeedRpm: maxSpeedRpmEl ? parseFloat(maxSpeedRpmEl.value) || 3000 : 3000,
                speedUpTimeMs: speedUpTimeEl ? parseInt(speedUpTimeEl.value) || 1000 : 1000,
                speedDownTimeMs: speedDownTimeEl ? parseInt(speedDownTimeEl.value) || 1000 : 1000,
                maxTimeOverloadedMs: maxTimeOverloadedEl ? parseInt(maxTimeOverloadedEl.value) || 5000 : 5000,
                jamMin: jamMinEl ? parseFloat(jamMinEl.value) || 0.5 : 0.5,
                jamDetectionThreshold: jamDetectionEl ? parseFloat(jamDetectionEl.value) || 2.0 : 2.0,
                cellsInSeries: cellsInSeriesEl ? parseInt(cellsInSeriesEl.value) || 6 : 6,
                ledBarNum: ledBarNumEl ? parseInt(ledBarNumEl.value) || 10 : 10,
                ledBarBrightness: ledBarBrightnessEl ? parseInt(ledBarBrightnessEl.value) || 50 : 50,
                ledBarBrightnessSecond: ledBarBrightnessSecondEl ? parseInt(ledBarBrightnessSecondEl.value) || 25 : 25,
                ledFrequency: ledFrequencyEl ? parseInt(ledFrequencyEl.value) || 800 : 800,
                lampMaxLevels: maxLevels,
                lampBrightness: lampBrightness,
                wifiSSID: wifiSSIDEl ? wifiSSIDEl.value || '' : '',
                wifiPassword: wifiPasswordEl ? wifiPasswordEl.value || '' : '',
                beeperEnabled: beeperEnabledEl ? beeperEnabledEl.checked : false,
                debugLoggingEnabled: debugLoggingEnabledEl ? debugLoggingEnabledEl.checked : false,
                standbyBlinkStartMinutes: standbyBlinkStartEl ? parseInt(standbyBlinkStartEl.value) || 5 : 5,
                standbyBlinkDurationSeconds: standbyBlinkDurationEl ? parseInt(standbyBlinkDurationEl.value) || 10 : 10
            };
            
            console.log('Settings data to be sent:', settingsData);
            
            fetch('/api/settings', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify(settingsData)
            })
            .then(response => {
                console.log('Save settings response status:', response.status);
                if (!response.ok) {
                    throw new Error('Network response was not ok: ' + response.status);
                }
                return response.json();
            })
            .then(data => {
                console.log('Save settings response data:', data);
                const settingsStatusEl = document.getElementById('settingsStatus');
                if (settingsStatusEl) {
                    if (data.success) {
                        settingsStatusEl.textContent = 'Settings saved successfully!';
                        console.log('Settings saved successfully on server');
                        
                        // Update remote control lamp slider interface after successful save
                        updateLampSliderInterface();
                        
                        // Update remote control lamp level interface after successful save
                        updateLampLevelInterface();
                    } else {
                        settingsStatusEl.textContent = 'Error saving settings!';
                        console.error('Server reported error saving settings');
                    }
                }
            })
            .catch(error => {
                console.error('Error saving settings:', error);
                const settingsStatusEl = document.getElementById('settingsStatus');
                if (settingsStatusEl) {
                    settingsStatusEl.textContent = 'Error saving settings: ' + error.message;
                }
            });
        }
        
        // Restore default settings
        function restoreDefaultSettings() {
            if (confirm('Are you sure you want to restore default settings? This will overwrite all current settings.')) {
                const settingsStatusEl = document.getElementById('settingsStatus');
                if (settingsStatusEl) settingsStatusEl.textContent = 'Restoring defaults...';
                
                fetch('/api/settings/restore', {
                    method: 'POST'
                })
                .then(response => response.json())
                .then(data => {
                    const settingsStatusEl = document.getElementById('settingsStatus');
                    if (settingsStatusEl) {
                        if (data.success) {
                            settingsStatusEl.textContent = 'Default settings restored!';
                            loadDPVSettings(); // Reload settings from server
                        } else {
                            settingsStatusEl.textContent = 'Error restoring defaults!';
                        }
                    }
                })
                .catch(error => {
                    console.error('Error restoring defaults:', error);
                    const settingsStatusEl = document.getElementById('settingsStatus');
                    if (settingsStatusEl) {
                        settingsStatusEl.textContent = 'Error restoring defaults!';
                    }
                });
            }
        }
        
        // Export settings to JSON file
        function exportSettings() {
            document.getElementById('settingsStatus').textContent = 'Exporting settings...';
            
            fetch('/api/settings')
                .then(response => response.json())
                .then(data => {
                    const exportData = {
                        exportInfo: {
                            version: "1.0",
                            timestamp: new Date().toISOString(),
                            device: "DPVControl"
                        },
                        settings: data
                    };
                    
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
            
            const reader = new FileReader();
            reader.onload = function(e) {
                try {
                    const importData = JSON.parse(e.target.result);
                    let settingsToImport = importData.settings || importData;
                    
                    fetch('/api/settings', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify(settingsToImport)
                    })
                    .then(response => response.json())
                    .then(data => {
                        if (data.success) {
                            document.getElementById('settingsStatus').textContent = 'Settings imported successfully!';
                            setTimeout(() => loadDPVSettings(), 1000);
                        } else {
                            document.getElementById('settingsStatus').textContent = 'Error importing settings!';
                        }
                    });
                } catch (error) {
                    document.getElementById('settingsStatus').textContent = 'Error: Invalid file format!';
                }
            };
            reader.readAsText(file);
            event.target.value = '';
        }
        
        // Tab Navigation
        function showTab(tabName) {
            document.querySelectorAll('.tab-content').forEach(tab => {
                tab.classList.remove('active');
            });
            document.getElementById(tabName + '-tab').classList.add('active');
            
            document.querySelectorAll('.nav-tab').forEach(btn => {
                btn.classList.remove('active');
            });
            event.target.classList.add('active');
        }
        
        // Remote Control Variables
        let motorRunning = false;
        let currentMotorSpeed = 0;
        
        // Toggle motor
        function toggleMotor() {
            motorRunning = !motorRunning;
            const button = document.getElementById('motorToggle');
            
            if (motorRunning) {
                button.textContent = 'STOP MOTOR';
                button.style.backgroundColor = '#f44336';
            } else {
                button.textContent = 'START MOTOR';
                button.style.backgroundColor = '#4caf50';
                setMotorSpeed(0);
            }
        }
        
        // Update motor speed display
        function updateMotorSpeed(value) {
            currentMotorSpeed = parseInt(value);
            document.getElementById('motorSpeedValue').textContent = currentMotorSpeed;
        }
        
        // Set motor speed
        function setMotorSpeed(value) {
            currentMotorSpeed = parseInt(value);
            updateMotorSpeed(value);
            
            const enabled = motorRunning && currentMotorSpeed > 0;
            
            fetch('/api/motor', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ enabled: enabled, speed: motorRunning ? currentMotorSpeed : 0 })
            })
            .then(response => response.json())
            .then(data => {
                console.log('Motor control response:', data);
            })
            .catch(error => {
                console.error('Error controlling motor:', error);
            });
        }
        
        // Update lamp level display with dynamic level calculation
        function updateLampLevel(value) {
            const maxLevels = parseInt(document.getElementById('lampMaxLevels')?.value || 5);
            const levelPercent = parseInt(value);
            
            // Convert percentage (0-100) to actual LED level (0-maxLevels)
            let actualLevel = 0;
            if (levelPercent > 0) {
                // Map 1-100% to levels 1 through (maxLevels-1) dynamically
                // For example: 3 levels (OFF + 2 brightness): 1-50% -> level 1, 51-100% -> level 2
                // For example: 5 levels (OFF + 4 brightness): 1-25% -> level 1, 26-50% -> level 2, etc.
                const activeLevels = maxLevels - 1; // Exclude level 0 (OFF)
                if (activeLevels > 0) {
                    // Calculate which level based on percentage
                    actualLevel = Math.floor(((levelPercent - 1) * activeLevels) / 100) + 1;
                    if (actualLevel < 1) actualLevel = 1;
                    if (actualLevel >= maxLevels) actualLevel = maxLevels - 1;
                } else {
                    actualLevel = 1; // Fallback to level 1 if no active levels configured
                }
            }
            
            document.getElementById('lampLevelValue').textContent = levelPercent;
            
            let statusText;
            if (levelPercent === 0) {
                statusText = 'OFF';
            } else if (levelPercent === 100) {
                statusText = 'MAX';
            } else {
                statusText = 'Level ' + actualLevel;
            }
            
            document.getElementById('lampLevelName').textContent = statusText;
            
            console.log('Lamp: ' + levelPercent + '% -> Level ' + actualLevel + ' of ' + maxLevels);
        }
        
        // Set lamp level with improved percentage to level mapping
        function setLampLevel(value) {
            const maxLevels = parseInt(document.getElementById('lampMaxLevels')?.value || 5);
            const levelPercent = parseInt(value);
            updateLampLevel(value);
            
            fetch('/api/lamp', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ level: levelPercent })
            })
            .then(response => response.json())
            .then(data => {
                console.log('Lamp control response:', data);
                document.getElementById('remoteLastCommand').textContent = 'Lamp: ' + levelPercent + '%';
                document.getElementById('remoteLampStatus').textContent = levelPercent === 0 ? 'OFF' : levelPercent + '%';
            })
            .catch(error => {
                console.error('Error controlling lamp:', error);
            });
        }
        
        // Emergency stop function
        function emergencyStop() {
            console.log('Emergency stop initiated');
            motorRunning = false;
            currentMotorSpeed = 0;
            
            const button = document.getElementById('motorToggle');
            button.textContent = 'START MOTOR';
            button.style.backgroundColor = '#4caf50';
            
            document.getElementById('motorSpeedSlider').value = 0;
            updateMotorSpeed(0);
            
            // Send emergency stop to API
            fetch('/api/motor', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ enabled: false, speed: 0 })
            })
            .then(response => response.json())
            .then(data => {
                console.log('Emergency stop response:', data);
                document.getElementById('remoteControlStatus').textContent = 'Emergency stop executed!';
                document.getElementById('remoteControlStatus').style.color = '#f44336';
            })
            .catch(error => {
                console.error('Error during emergency stop:', error);
            });
        }
        
        // System reboot function
        function rebootSystem() {
            if (confirm('Are you sure you want to reboot the DPV Control System? This will interrupt any active sessions.')) {
                document.getElementById('settingsStatus').textContent = 'Rebooting system...';
                
                fetch('/api/reboot', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' }
                })
                .then(response => response.json())
                .then(data => {
                    document.getElementById('settingsStatus').textContent = 'System is rebooting. Please wait 30 seconds and refresh the page.';
                    
                    // Show countdown
                    let countdown = 30;
                    const countdownInterval = setInterval(() => {
                        countdown--;
                        document.getElementById('settingsStatus').textContent = 
                            'System is rebooting. Refresh page in ' + countdown + ' seconds...';
                        
                        if (countdown <= 0) {
                            clearInterval(countdownInterval);
                            document.getElementById('settingsStatus').textContent = 'Please refresh the page manually.';
                        }
                    }, 1000);
                })
                .catch(error => {
                    console.error('Error rebooting system:', error);
                    document.getElementById('settingsStatus').textContent = 'Error rebooting system!';
                });
            }
        }
        
        // Delete all sessions function
        function deleteAllSessions() {
            if (confirm('Are you sure you want to delete ALL session data? This cannot be undone!')) {
                document.getElementById('sessionDurationInfo').textContent = 'Deleting sessions...';
                
                fetch('/api/delete-all-sessions', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' }
                })
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        document.getElementById('sessionDurationInfo').textContent = 
                            'Deleted ' + (data.deleted || 0) + ' session files';
                        
                        // Reload session list
                        loadSessionList();
                        
                        // Clear chart
                        allDataPoints = [];
                        updateCharts([]);
                    } else {
                        document.getElementById('sessionDurationInfo').textContent = 
                            'Error deleting sessions: ' + (data.error || 'Unknown error');
                    }
                })
                .catch(error => {
                    console.error('Error deleting sessions:', error);
                    document.getElementById('sessionDurationInfo').textContent = 'Error deleting sessions!';
                });
            }
        }
        
        // Export functions for charts
        function exportCurrentViewAsCSV() {
            if (!allDataPoints || allDataPoints.length === 0) {
                alert('No data available to export');
                return;
            }
            
            // Filter data by current time window
            const filteredData = filterDataByTimeRange(allDataPoints);
            
            if (filteredData.length === 0) {
                alert('No data in current view to export');
                return;
            }
            
            // Generate filename with session and timestamp
            const sessionName = selectedSession ? selectedSession.replace('.bin', '') : 'current';
            const timestamp = new Date().toISOString().slice(0, 19).replace(/:/g, '-');
            const filename = `${sessionName}_view_${timestamp}.csv`;
            
            exportToCSV(filteredData, filename);
        }
        
        function exportAllSessionsAsZip() {
            if (!selectedSession) {
                alert('No session selected');
                return;
            }
            
            if (!allDataPoints || allDataPoints.length === 0) {
                alert('No data available to export');
                return;
            }
            
            // For now, export current session as CSV (JSZip fallback)
            const sessionName = selectedSession.replace('.bin', '');
            const timestamp = new Date().toISOString().slice(0, 19).replace(/:/g, '-');
            const filename = `${sessionName}_complete_${timestamp}.csv`;
            
            exportToCSV(allDataPoints, filename);
        }
        
        // Update lamp slider interface function (for settings compatibility)
        function updateLampSliderInterface() {
            // Call the new function name
            updateRemoteLampSlider();
        }
        
        // Lamp control state
        let lampIsOn = false;
        let currentLampLevel = 1; // Default to level 1 when turning on
        
        // Toggle lamp ON/OFF
        function toggleLamp() {
            const button = document.getElementById('lampToggleBtn');
            const container = document.getElementById('lampLevelsContainer');
            const radioButtons = document.querySelectorAll('input[name="lampLevel"]');
            
            if (!lampIsOn) {
                // Turn lamp ON
                lampIsOn = true;
                button.textContent = '💡 LAMP ON';
                button.style.backgroundColor = '#4caf50';
                container.style.opacity = '1';
                
                // Enable radio buttons
                radioButtons.forEach(radio => {
                    radio.disabled = false;
                });
                
                // Select the current level radio button
                const currentRadio = document.querySelector(`input[name="lampLevel"][value="${currentLampLevel}"]`);
                if (currentRadio) {
                    currentRadio.checked = true;
                }
                
                // Set lamp to current level
                setLampLevelDirect(currentLampLevel);
            } else {
                // Turn lamp OFF
                lampIsOn = false;
                button.textContent = '💡 LAMP OFF';
                button.style.backgroundColor = '#666';
                container.style.opacity = '0.5';
                
                // Disable radio buttons and uncheck all
                radioButtons.forEach(radio => {
                    radio.disabled = true;
                    radio.checked = false;
                });
                
                // Turn off lamp
                setLampLevelDirect(0);
            }
        }
        
        // Set lamp level from radio button selection
        function setLampLevelFromRadio(level) {
            currentLampLevel = parseInt(level);
            setLampLevelDirect(currentLampLevel);
        }
        
        // Direct lamp level control (internal function)
        function setLampLevelDirect(level) {
            fetch('/api/lamp', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ level: level })
            })
            .then(response => response.json())
            .then(data => {
                console.log('Lamp control response:', data);
                const levelText = level === 0 ? 'OFF' : `Level ${level}`;
                document.getElementById('remoteLastCommand').textContent = `Lamp: ${levelText}`;
                document.getElementById('remoteLampStatus').textContent = levelText;
            })
            .catch(error => {
                console.error('Error controlling lamp:', error);
            });
        }
        
        // Update remote lamp controls based on current settings (replaces slider interface)
        function updateRemoteLampSlider() {
            // Update the radio button labels based on settings
            const maxLevels = parseInt(document.getElementById('lampMaxLevels')?.value || 5);
            console.log('Updated lamp controls for maxLevels=' + maxLevels);
            
            // Reset lamp state to safe defaults
            lampIsOn = false;
            currentLampLevel = 1;
            
            const button = document.getElementById('lampToggleBtn');
            const container = document.getElementById('lampLevelsContainer');
            if (button && container) {
                button.textContent = '💡 LAMP OFF';
                button.style.backgroundColor = '#666';
                container.style.opacity = '0.5';
                
                // Disable all radio buttons initially
                const radioButtons = document.querySelectorAll('input[name="lampLevel"]');
                radioButtons.forEach(radio => {
                    radio.disabled = true;
                    radio.checked = false;
                });
            }
        }
        
        // Legacy function for backward compatibility
        function updateLampLevel(value) {
            console.log('Legacy updateLampLevel called with value:', value);
            // Convert old percentage-based calls to new level-based system
            if (value == 0) {
                if (lampIsOn) toggleLamp(); // Turn off if currently on
            } else {
                // Calculate level from percentage
                const level = Math.ceil((parseInt(value) / 100) * 4); // Map to levels 1-4
                currentLampLevel = Math.max(1, Math.min(4, level));
                if (!lampIsOn) toggleLamp(); // Turn on if currently off
                else setLampLevelFromRadio(currentLampLevel); // Update level
            }
        }
        
        // Legacy function for backward compatibility  
        function setLampLevel(value) {
            updateLampLevel(value);
        }
        
        // Generate dynamic lamp level radio buttons based on current settings
        function updateLampLevelInterface() {
            const maxLevels = parseInt(document.getElementById('lampMaxLevels')?.value || 5);
            const container = document.getElementById('lampLevelRadios');
            
            if (!container) {
                console.warn('lampLevelRadios container not found');
                return;
            }
            
            // Clear existing radio buttons
            container.innerHTML = '';
            
            // Generate radio buttons for each level (starting from level 1)
            for (let i = 1; i < maxLevels; i++) {
                const brightnessEl = document.getElementById('lampBrightness' + i);
                const brightness = brightnessEl ? parseInt(brightnessEl.value) || 0 : 0;
                
                const label = document.createElement('label');
                label.style.cssText = 'display: flex; align-items: center; cursor: pointer;';
                
                const radio = document.createElement('input');
                radio.type = 'radio';
                radio.name = 'lampLevel';
                radio.value = i.toString();
                radio.style.cssText = 'margin-right: 10px;';
                radio.onchange = function() { setLampLevelFromRadio(this.value); };
                radio.disabled = false; // Enable radio buttons when interface is ready
                
                const span = document.createElement('span');
                span.textContent = `Level ${i} - ${brightness}`;
                
                label.appendChild(radio);
                label.appendChild(span);
                container.appendChild(label);
            }
            
            console.log('Updated lamp level interface with', maxLevels - 1, 'brightness levels');
        }
        
        // Enable remote control interface after system initialization
        function enableRemoteControlInterface() {
            console.log('Enabling remote control interface...');
            
            // Enable motor control slider
            const motorSlider = document.getElementById('motorSpeed');
            if (motorSlider) {
                motorSlider.disabled = false;
                motorSlider.style.opacity = '1.0';
            }
            
            // Enable lamp control slider
            const lampSlider = document.getElementById('lampLevelSlider');
            if (lampSlider) {
                lampSlider.disabled = false;
                lampSlider.style.opacity = '1.0';
            }
            
            // Enable all lamp level radio buttons
            const radioButtons = document.querySelectorAll('input[name="lampLevel"]');
            radioButtons.forEach(radio => {
                radio.disabled = false;
            });
            
            // Enable motor toggle button
            const motorToggle = document.getElementById('motorToggle');
            if (motorToggle) {
                motorToggle.disabled = false;
                motorToggle.style.opacity = '1.0';
            }
            
            console.log('Remote control interface enabled');
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
    
    // If no data available, return empty array
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
        json += "\"rpm\":" + String(dataPoints[i].erpm) + ",";
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
    
    // Debug: Show first part of JSON
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
        return "";
    }
    
    File tripFile = LittleFS.open("/trip_log.bin", "r");
    if (!tripFile) {
        log("Failed to open trip log file");
        return "";
    }
    
    size_t fileSize = tripFile.size();
    size_t dataPointCount = fileSize / sizeof(LogdataRow);
    
    String countMsg = "Trip log contains " + String(dataPointCount) + " data points (" + String(fileSize) + " bytes)";
    log(countMsg.c_str());
    
    if (dataPointCount == 0) {
        tripFile.close();
        return "";
    }
    
    // CSV Header
    String csv = "Timestamp,Motor Temperature (degC),MOSFET Temperature (degC),Battery Voltage (V),Input Current (A),Motor Current (A),RPM,Duty Cycle (%),Ambient Temperature (degC),Humidity (%),Battery Level (%),Leak Sensor State,LED State,Total Uptime (s)\r\n";
    
    LogdataRow dataPoint;
    
    // Read and convert each data point
    for (size_t i = 0; i < dataPointCount; i++) {
        size_t bytesRead = tripFile.read((uint8_t*)&dataPoint, sizeof(LogdataRow));
        
        if (bytesRead != sizeof(LogdataRow)) {
            String errorMsg = "Error reading data point " + String(i) + ", bytes read: " + String(bytesRead);
            log(errorMsg.c_str());
            break;
        }
        
        // Convert timestamp to ISO format
        time_t timestamp = dataPoint.timestamp / 1000; // Convert to seconds
        struct tm* timeinfo = gmtime(&timestamp);
        char timeStr[30];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%dT%H:%M:%S.000Z", timeinfo);
        
        // Add data row
        csv += String(timeStr) + ",";
        csv += String(dataPoint.tempMotor) + ",";
        csv += String(dataPoint.tempMosfet) + ",";
        csv += String(dataPoint.batteryVoltage) + ",";
        csv += String(dataPoint.current) + ",";
        csv += String(dataPoint.avgMotorCurrent) + ",";
        csv += String(dataPoint.erpm) + ",";
        csv += String(dataPoint.dutyCycle) + ",";
        csv += String(dataPoint.temperature) + ",";
        csv += String(dataPoint.humidity) + ",";
        csv += String(dataPoint.batteryLevel) + ",";
        csv += String(dataPoint.leakSensorState) + ",";
        csv += String(dataPoint.ledState) + ",";
        csv += String(dataPoint.totalUptime) + "\r\n";
        
        // Prevent memory overflow for very large files
        if (csv.length() > 50000) { // Limit to ~50KB
            String limitMsg = "CSV size limit reached at " + String(i+1) + " points, truncating";
            log(limitMsg.c_str());
            break;
        }
    }
    
    tripFile.close();
    
    String resultMsg = "Generated full trip log CSV, length: " + String(csv.length()) + " for " + String(dataPointCount) + " points";
    log(resultMsg.c_str());
    
    return csv;
}

/**
 * Generate JSON for current DPV settings
 */
String generateSettingsJson() {
    log("generateSettingsJson called");
    
    JsonDocument doc;
    
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
    JsonArray lampBrightness = doc["lampBrightness"].to<JsonArray>();
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
    String logMsg = "JSON length: " + String(jsonString.length());
    log(logMsg.c_str());
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        String errorMsg = "Failed to parse settings JSON: " + String(error.c_str());
        log(errorMsg.c_str());
        return false;
    }
    
    log("JSON parsed successfully");
    
    // Create temporary settings structure
    DPVSettings newSettings = currentSettings;
    
    // Log current values before update
    String currentMsg = "Current speedSteps: " + String(currentSettings.speedSteps) + 
                       ", standbyDelay: " + String(currentSettings.standbyDelaySeconds) +
                       ", beeperEnabled: " + String(currentSettings.beeperEnabled ? "true" : "false");
    log(currentMsg.c_str());
    
    // Update settings from JSON with detailed logging
    int updatedFields = 0;
    if (doc["speedSteps"].is<int>()) {
        int oldVal = newSettings.speedSteps;
        newSettings.speedSteps = doc["speedSteps"];
        String msg = "Updated speedSteps: " + String(oldVal) + " -> " + String(newSettings.speedSteps);
        log(msg.c_str());
        updatedFields++;
    }
    if (doc["standbyDelaySeconds"].is<int>()) {
        int oldVal = newSettings.standbyDelaySeconds;
        newSettings.standbyDelaySeconds = doc["standbyDelaySeconds"];
        String msg = "Updated standbyDelaySeconds: " + String(oldVal) + " -> " + String(newSettings.standbyDelaySeconds);
        log(msg.c_str());
        updatedFields++;
    }
    if (doc["batteryPowerMax"].is<int>()) {
        int oldVal = newSettings.batteryPowerMax;
        newSettings.batteryPowerMax = doc["batteryPowerMax"];
        String msg = "Updated batteryPowerMax: " + String(oldVal) + " -> " + String(newSettings.batteryPowerMax);
        log(msg.c_str());
        updatedFields++;
    }
    if (doc["minSpeedPercent"].is<float>()) {
        float oldVal = newSettings.minSpeedPercent;
        newSettings.minSpeedPercent = doc["minSpeedPercent"];
        String msg = "Updated minSpeedPercent: " + String(oldVal, 3) + " -> " + String(newSettings.minSpeedPercent, 3);
        log(msg.c_str());
        updatedFields++;
    }
    if (doc["maxSpeedRpm"].is<float>()) {
        float oldVal = newSettings.maxSpeedRpm;
        newSettings.maxSpeedRpm = doc["maxSpeedRpm"];
        String msg = "Updated maxSpeedRpm: " + String(oldVal, 1) + " -> " + String(newSettings.maxSpeedRpm, 1);
        log(msg.c_str());
        updatedFields++;
    }
    if (doc["speedUpTimeMs"].is<int>()) {
        int oldVal = newSettings.speedUpTimeMs;
        newSettings.speedUpTimeMs = doc["speedUpTimeMs"];
        String msg = "Updated speedUpTimeMs: " + String(oldVal) + " -> " + String(newSettings.speedUpTimeMs);
        log(msg.c_str());
        updatedFields++;
    }
    if (doc["speedDownTimeMs"].is<int>()) {
        int oldVal = newSettings.speedDownTimeMs;
        newSettings.speedDownTimeMs = doc["speedDownTimeMs"];
        String msg = "Updated speedDownTimeMs: " + String(oldVal) + " -> " + String(newSettings.speedDownTimeMs);
        log(msg.c_str());
        updatedFields++;
    }
    if (doc["maxTimeOverloadedMs"].is<long>()) {
        long oldVal = newSettings.maxTimeOverloadedMs;
        newSettings.maxTimeOverloadedMs = doc["maxTimeOverloadedMs"];
        String msg = "Updated maxTimeOverloadedMs: " + String(oldVal) + " -> " + String(newSettings.maxTimeOverloadedMs);
        log(msg.c_str());
        updatedFields++;
    }
    
    if (doc["jamMin"].is<float>()) newSettings.jamMin = doc["jamMin"];
    if (doc["jamDetectionThreshold"].is<float>()) newSettings.jamDetectionThreshold = doc["jamDetectionThreshold"];
    
    if (doc["cellsInSeries"].is<int>()) newSettings.cellsInSeries = doc["cellsInSeries"];
    
    if (doc["ledBarNum"].is<int>()) newSettings.ledBarNum = doc["ledBarNum"];
    if (doc["ledBarBrightness"].is<int>()) newSettings.ledBarBrightness = doc["ledBarBrightness"];
    if (doc["ledBarBrightnessSecond"].is<int>()) newSettings.ledBarBrightnessSecond = doc["ledBarBrightnessSecond"];
    if (doc["ledFrequency"].is<int>()) newSettings.ledFrequency = doc["ledFrequency"];
    
    if (doc["lampMaxLevels"].is<int>()) {
        int oldVal = newSettings.lampMaxLevels;
        newSettings.lampMaxLevels = doc["lampMaxLevels"];
        String msg = "Updated lampMaxLevels: " + String(oldVal) + " -> " + String(newSettings.lampMaxLevels);
        log(msg.c_str());
        updatedFields++;
    }
    if (doc["lampBrightness"].is<JsonArray>()) {
        log("Processing lampBrightness array...");
        JsonArray lampArray = doc["lampBrightness"];
        String oldValues = "Old lampBrightness values: ";
        for (int i = 0; i < 10; i++) {
            oldValues += String(newSettings.lampBrightness[i]);
            if (i < 9) oldValues += ",";
        }
        log(oldValues.c_str());
        
        String arrayInfo = "JSON lampBrightness array size: " + String(lampArray.size());
        log(arrayInfo.c_str());
        
        for (int i = 0; i < 10 && i < lampArray.size(); i++) {
            int oldVal = newSettings.lampBrightness[i];
            newSettings.lampBrightness[i] = lampArray[i];
            String msg = "Updated lampBrightness[" + String(i) + "]: " + String(oldVal) + " -> " + String(newSettings.lampBrightness[i]);
            log(msg.c_str());
        }
        
        String newValues = "New lampBrightness values: ";
        for (int i = 0; i < 10; i++) {
            newValues += String(newSettings.lampBrightness[i]);
            if (i < 9) newValues += ",";
        }
        log(newValues.c_str());
        updatedFields++;
    }
    
    if (doc["wifiSSID"].is<const char*>()) {
        strncpy(newSettings.wifiSSID, doc["wifiSSID"], sizeof(newSettings.wifiSSID) - 1);
        newSettings.wifiSSID[sizeof(newSettings.wifiSSID) - 1] = '\0';
    }
    if (doc["wifiPassword"].is<const char*>()) {
        strncpy(newSettings.wifiPassword, doc["wifiPassword"], sizeof(newSettings.wifiPassword) - 1);
        newSettings.wifiPassword[sizeof(newSettings.wifiPassword) - 1] = '\0';
    }
    
    if (doc["beeperEnabled"].is<bool>()) {
        bool oldVal = newSettings.beeperEnabled;
        newSettings.beeperEnabled = doc["beeperEnabled"];
        String msg = "Updated beeperEnabled: " + String(oldVal ? "true" : "false") + " -> " + String(newSettings.beeperEnabled ? "true" : "false");
        log(msg.c_str());
        updatedFields++;
    }
    if (doc["debugLoggingEnabled"].is<bool>()) {
        bool oldVal = newSettings.debugLoggingEnabled;
        newSettings.debugLoggingEnabled = doc["debugLoggingEnabled"];
        String msg = "Updated debugLoggingEnabled: " + String(oldVal ? "true" : "false") + " -> " + String(newSettings.debugLoggingEnabled ? "true" : "false");
        log(msg.c_str());
        updatedFields++;
    }
    if (doc["standbyBlinkStartMinutes"].is<int>()) {
        int oldVal = newSettings.standbyBlinkStartMinutes;
        newSettings.standbyBlinkStartMinutes = doc["standbyBlinkStartMinutes"];
        String msg = "Updated standbyBlinkStartMinutes: " + String(oldVal) + " -> " + String(newSettings.standbyBlinkStartMinutes);
        log(msg.c_str());
        updatedFields++;
    }
    if (doc["standbyBlinkDurationSeconds"].is<int>()) {
        int oldVal = newSettings.standbyBlinkDurationSeconds;
        newSettings.standbyBlinkDurationSeconds = doc["standbyBlinkDurationSeconds"];
        String msg = "Updated standbyBlinkDurationSeconds: " + String(oldVal) + " -> " + String(newSettings.standbyBlinkDurationSeconds);
        log(msg.c_str());
        updatedFields++;
    }
    
    String summaryMsg = "Total fields updated from JSON: " + String(updatedFields);
    log(summaryMsg.c_str());
    
    // Validate new settings
    log("Validating new settings...");
    if (!validateSettings(newSettings)) {
        log("ERROR: New settings failed validation!");
        return false;
    }
    log("Settings validation passed");
    
    // Apply new settings
    log("Applying new settings to currentSettings...");
    currentSettings = newSettings;
    
    log("Calling saveSettings()...");
    saveSettings();
    
    // Log the new effective lamp settings
    String lampInfo = "NEW LAMP SETTINGS APPLIED - MaxLevels: " + String(getLampMaxLevels());
    for (int i = 0; i <= getLampMaxLevels(); i++) {
        lampInfo += ", L" + String(i) + ":" + String(getLampBrightness(i));
    }
    log(lampInfo.c_str());
    
    // Log the new effective motor settings
    String motorInfo = "NEW MOTOR SETTINGS APPLIED - SpeedSteps: " + String(getSpeedSteps()) +
                      ", StandbyDelay: " + String(getStandbyDelay()) + "s" +
                      ", BatteryMax: " + String(getBatteryPowerMax()) + "A" +
                      ", MinSpeed: " + String(getMinSpeedPercent(), 2) +
                      ", MaxRPM: " + String(getMaxSpeedRpm(), 0);
    log(motorInfo.c_str());
    
    String motorInfo2 = "MOTOR TIMING - SpeedUp: " + String(getSpeedUpTime()) + "ms" +
                       ", SpeedDown: " + String(getSpeedDownTime()) + "ms" +
                       ", MaxOverload: " + String(getMaxTimeOverloaded()) + "ms";
    log(motorInfo2.c_str());
    
    String jamInfo = "JAM DETECTION - Min: " + String(getJamMin(), 2) +
                    ", Threshold: " + String(getJamDetectionThreshold(), 2);
    log(jamInfo.c_str());
    
    String otherInfo = "OTHER SETTINGS - Beeper: " + String(getBeeperEnabled() ? "ON" : "OFF") +
                      ", Debug: " + String(getDebugLoggingEnabled() ? "ON" : "OFF") +
                      ", LEDBar: " + String(getLedBarNum()) + " LEDs";
    log(otherInfo.c_str());
    
    log("Settings updated and saved successfully");
    return true;
}

/**
 * Generate CSV data for a specific session file
 */
String generateSessionCsvData(String sessionFile) {
    log(("Starting CSV generation for session: " + sessionFile).c_str());
    
    // Try to open the session file
    File file = LittleFS.open("/" + sessionFile, "r");
    if (!file) {
        String errorMsg = "Failed to open session file: " + sessionFile;
        log(errorMsg.c_str());
        return "Error: Could not open session file " + sessionFile;
    }
    
    // Calculate how many data points we have
    size_t fileSize = file.size();
    size_t dataPointCount = fileSize / sizeof(LogdataRow);
    
    String fileSizeMsg = "Session file size: " + String(fileSize) + " bytes, estimated " + String(dataPointCount) + " data points";
    log(fileSizeMsg.c_str());
    
    // Build CSV header with Total Uptime as primary time reference
    String csv = "Total Uptime (s),Motor Temperature (degC),MOSFET Temperature (degC),Battery Voltage (V),Input Current (A),Motor Current (A),RPM,Duty Cycle (%),Ambient Temperature (degC),Humidity (%),Battery Level (%),Leak Sensor State,LED State\\r\\n";
    
    LogdataRow dataPoint;
    int exportedPoints = 0;
    int maxPoints = 1000; // Reasonable limit for ESP32 memory
    
    // Read and convert each data point
    for (size_t i = 0; i < dataPointCount && exportedPoints < maxPoints; i++) {
        size_t bytesRead = file.read((uint8_t*)&dataPoint, sizeof(LogdataRow));
        
        if (bytesRead != sizeof(LogdataRow)) {
            String errorMsg = "Error reading data point " + String(i) + " from session " + sessionFile + ", bytes read: " + String(bytesRead);
            log(errorMsg.c_str());
            break;
        }
        
        // Convert timestamp to total uptime in seconds
        float totalUptimeSeconds = dataPoint.totalUptime / 1000.0;
        
        // Build CSV row with Total Uptime first
        csv += String(totalUptimeSeconds, 1) + ",";
        csv += String(dataPoint.tempMotor, 1) + ",";
        csv += String(dataPoint.tempMosfet, 1) + ",";
        csv += String(dataPoint.batteryVoltage, 5) + ",";
        csv += String(dataPoint.current, 2) + ",";
        csv += String(dataPoint.avgMotorCurrent, 2) + ",";
        csv += String(dataPoint.erpm) + ",";
        csv += String(dataPoint.dutyCycle, 3) + ",";
        csv += String(dataPoint.temperature, 1) + ",";
        csv += String(dataPoint.humidity, 1) + ",";
        csv += String(dataPoint.batteryLevel) + ",";
        csv += String(dataPoint.leakSensorState) + ",";
        csv += String(dataPoint.ledState) + "\\r\\n";
        
        exportedPoints++;
        
        // Check memory usage more frequently
        if (csv.length() > 400000) { // 400KB limit for ESP32 safety
            String limitMsg = "Session CSV memory limit reached at " + String(exportedPoints) + " points for " + sessionFile + ", exported " + String((float)exportedPoints/dataPointCount*100, 1) + "% of session";
            log(limitMsg.c_str());
            break;
        }
        
        // Give other tasks time to run
        if (i % 10 == 0) {
            vTaskDelay(1 / portTICK_PERIOD_MS);
        }
    }
    
    file.close();
    
    String resultMsg = "Generated session CSV for " + sessionFile + " with " + String(exportedPoints) + "/" + String(dataPointCount) + " points, size: " + String(csv.length()) + " bytes";
    log(resultMsg.c_str());
    
    // Add summary footer if truncated
    if (exportedPoints < dataPointCount) {
        csv += "\\r\\n# Note: Session truncated due to memory limits\\r\\n";
        csv += "# Exported " + String(exportedPoints) + " of " + String(dataPointCount) + " total points (" + String((float)exportedPoints/dataPointCount*100, 1) + "%)\\r\\n";
        csv += "# Use session data API for complete dataset\\r\\n";
    }
    
    return csv;
}

/**
 * Stream CSV data directly to client to avoid memory issues
 */
void streamSessionCsvData(WiFiClient client, String sessionFile) {
    log(("Starting streaming CSV for session: " + sessionFile).c_str());
    
    // Try to open the session file
    File file = LittleFS.open("/" + sessionFile, "r");
    if (!file) {
        String errorMsg = "Failed to open session file: " + sessionFile;
        log(errorMsg.c_str());
        String errorResponse = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nError: Could not open session file " + sessionFile;
        client.print(errorResponse);
        return;
    }
    
    // Calculate how many data points we have
    size_t fileSize = file.size();
    size_t dataPointCount = fileSize / sizeof(LogdataRow);
    
    String fileSizeMsg = "Session file size: " + String(fileSize) + " bytes, estimated " + String(dataPointCount) + " data points";
    log(fileSizeMsg.c_str());
    
    // Send HTTP headers for CSV download
    client.print("HTTP/1.1 200 OK\r\n");
    client.print("Content-Type: text/csv\r\n");
    client.print("Content-Disposition: attachment; filename=\"" + sessionFile.substring(0, sessionFile.lastIndexOf('.')) + ".csv\"\r\n");
    client.print("Cache-Control: no-cache\r\n");
    client.print("\r\n");
    
    // Send CSV header
    client.print("Total Uptime (s),Motor Temperature (degC),MOSFET Temperature (degC),Battery Voltage (V),Input Current (A),Motor Current (A),RPM,Duty Cycle (%),Ambient Temperature (degC),Humidity (%),Battery Level (%),Leak Sensor State,LED State\r\n");
    
    LogdataRow dataPoint;
    int exportedPoints = 0;
    int maxPoints = 10000; // Much higher limit since we're streaming
    
    // Stream each data point directly to client
    for (size_t i = 0; i < dataPointCount && exportedPoints < maxPoints; i++) {
        size_t bytesRead = file.read((uint8_t*)&dataPoint, sizeof(LogdataRow));
        
        if (bytesRead != sizeof(LogdataRow)) {
            String errorMsg = "Error reading data point " + String(i) + " from session " + sessionFile;
            log(errorMsg.c_str());
            break;
        }
        
        // Convert timestamp to total uptime in seconds
        float totalUptimeSeconds = dataPoint.totalUptime / 1000.0;
        
        // Build and send CSV row directly (small string, immediately sent)
        String csvRow = String(totalUptimeSeconds, 1) + ",";
        csvRow += String(dataPoint.tempMotor, 1) + ",";
        csvRow += String(dataPoint.tempMosfet, 1) + ",";
        csvRow += String(dataPoint.batteryVoltage, 5) + ",";
        csvRow += String(dataPoint.current, 2) + ",";
        csvRow += String(dataPoint.avgMotorCurrent, 2) + ",";
        csvRow += String(dataPoint.erpm) + ",";
        csvRow += String(dataPoint.dutyCycle, 3) + ",";
        csvRow += String(dataPoint.temperature, 1) + ",";
        csvRow += String(dataPoint.humidity, 1) + ",";
        csvRow += String(dataPoint.batteryLevel) + ",";
        csvRow += String(dataPoint.leakSensorState) + ",";
        csvRow += String(dataPoint.ledState) + "\r\n";
        
        // Send this row immediately
        client.print(csvRow);
        
        exportedPoints++;
        
        // Give other tasks time and check client connection
        if (i % 10 == 0) {
            vTaskDelay(1 / portTICK_PERIOD_MS);
            if (!client.connected()) {
                log("Client disconnected during CSV stream");
                break;
            }
        }
        
        // Progress logging
        if (exportedPoints % 100 == 0) {
            String progressMsg = "Streamed " + String(exportedPoints) + "/" + String(dataPointCount) + " CSV rows";
            log(progressMsg.c_str());
        }
    }
    
    file.close();
    
    // Send summary footer if truncated
    if (exportedPoints < dataPointCount) {
        client.print("\r\n# Note: Session truncated due to data limits\r\n");
        client.print("# Exported " + String(exportedPoints) + " of " + String(dataPointCount) + " total points (" + String((float)exportedPoints/dataPointCount*100, 1) + "%)\r\n");
    }
    
    String resultMsg = "Completed streaming CSV for " + sessionFile + " with " + String(exportedPoints) + "/" + String(dataPointCount) + " points";
    log(resultMsg.c_str());
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
        jsonStatus += "\"beeperEnabled\":" + String(currentSettings.beeperEnabled ? "true" : "false") + ",";
        
        // Add motor data
        jsonStatus += "\"erpm\":" + String(getVescUart().data.rpm) + ",";
        jsonStatus += "\"motorTemp\":" + String(getVescUart().data.tempMotor) + ",";
        jsonStatus += "\"mosfetTemp\":" + String(getVescUart().data.tempMosfet) + ",";
        jsonStatus += "\"inputVoltage\":" + String(getVescUart().data.inpVoltage) + ",";
        jsonStatus += "\"avgInputCurrent\":" + String(getVescUart().data.avgInputCurrent) + ",";
        jsonStatus += "\"avgMotorCurrent\":" + String(getVescUart().data.avgMotorCurrent) + ",";
        jsonStatus += "\"dutyCycle\":" + String(getVescUart().data.dutyCycleNow) + ",";
        jsonStatus += "\"motorState\":" + String(static_cast<int>(motorState));
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
        
    } else if (path.startsWith("/api/session-csv?")) {
        // API endpoint for getting CSV data from a specific session
        log("API /api/session-csv called");
        
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
        
        streamSessionCsvData(client, sessionFile);
        
    } else if (path == "/api/trip-log") {
        // API endpoint for full trip log download
        log("API /api/trip-log called");
        
        String csvData = generateFullTripLogJson();
        
        String responseMsg = "Sending full trip log, length: " + String(csvData.length());
        log(responseMsg.c_str());
        
        sendHttpResponse(client, 200, "text/csv", csvData.c_str());
        
    } else if (path == "/api/settings" && method == "GET") {
        // API endpoint to get current settings
        log("API /api/settings GET called");
        
        String jsonSettings = generateSettingsJson();
        sendHttpResponse(client, 200, "application/json", jsonSettings.c_str());
        
    } else if (path == "/api/settings" && method == "POST") {
        // API endpoint to save settings
        log("API /api/settings POST called");
        
        // Read POST body using Content-Length if available
        String body = "";
        if (contentLength.length() > 0) {
            int bodyLength = contentLength.toInt();
            if (bodyLength > 0 && bodyLength < 4096) { // Reasonable limit for settings JSON
                char* buffer = new char[bodyLength + 1];
                int bytesRead = 0;
                unsigned long startTime = millis();
                
                // Read the exact number of bytes specified in Content-Length
                while (bytesRead < bodyLength && client.connected() && (millis() - startTime < 5000)) {
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
                
                String readMsg = "Settings: Read " + String(bytesRead) + " bytes of " + String(bodyLength) + " expected";
                log(readMsg.c_str());
            } else {
                log("Settings: Invalid Content-Length or too large");
            }
        } else {
            // Fallback: read whatever is available (old method)
            delay(100); // Give more time for settings data to arrive
            while (client.available()) {
                body += (char)client.read();
            }
            log("Settings: Using fallback reading method");
        }
        
        String bodyMsg = "Settings POST body received, length: " + String(body.length());
        log(bodyMsg.c_str());
        
        if (body.length() > 100) {
            String bodyPreview = "Settings body preview: " + body.substring(0, 100) + "...";
            log(bodyPreview.c_str());
        } else if (body.length() > 0) {
            String bodyFull = "Settings body full: " + body;
            log(bodyFull.c_str());
        } else {
            log("Settings: ERROR - No body data received!");
        }
        
        bool success = false;
        if (body.length() > 0) {
            success = updateSettingsFromJson(body);
        } else {
            log("Settings: Cannot save - empty body");
        }
        
        String response = "{\"success\":" + String(success ? "true" : "false") + "}";
        sendHttpResponse(client, 200, "application/json", response.c_str());
        
    } else if (path == "/api/settings/restore" && method == "POST") {
        // API endpoint to restore default settings
        log("API /api/settings/restore called");
        
        restoreDefaultSettings();
        
        String response = "{\"success\":true}";
        sendHttpResponse(client, 200, "application/json", response.c_str());
        
    } else if (path == "/api/reboot" && method == "POST") {
        // API endpoint to reboot the system
        log("API /api/reboot called");
        
        String response = "{\"success\":true,\"message\":\"Reboot initiated\"}";
        sendHttpResponse(client, 200, "application/json", response.c_str());
        
        // Close client connection properly before rebooting
        client.stop();
        
        // Wait a moment to ensure response is sent
        delay(500);
        
        // Reboot the ESP32
        log("System reboot requested via API - restarting now");
        ESP.restart();
        
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
            
            // Convert speed percentage (0-100) to motor steps (1-maxSteps)
            int maxSteps = getSpeedSteps();
            int targetStep = max(1, min(maxSteps, (speed * maxSteps) / 100));
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
        
        // Extract level value (now direct level 0-maxLevels)
        int requestedLevel = 0;
        int levelIndex = body.indexOf("\"level\":");
        if (levelIndex != -1) {
            String levelStr = body.substring(levelIndex + 8);
            int endIndex = levelStr.indexOf(',');
            if (endIndex == -1) endIndex = levelStr.indexOf('}');
            if (endIndex != -1) {
                levelStr = levelStr.substring(0, endIndex);
                requestedLevel = levelStr.toInt();
            }
        }
        
        // Validate level against current settings
        int maxLevels = getLampMaxLevels();
        int actualLevel = requestedLevel;
        
        // Validate level range
        if (actualLevel < 0) actualLevel = 0;
        if (actualLevel >= maxLevels) actualLevel = maxLevels - 1;
        
        // Integrate with actual LED lamp functions
        String controlMsg = "Remote lamp control - Requested Level: " + String(requestedLevel) + ", Actual Level: " + String(actualLevel);
        log(controlMsg.c_str());
        
        // Set level
        LED_State = actualLevel;
        setLEDState(LED_State);
        setBarLED(LED_State);
        
        String levelMsg = "Remote control set lamp to level " + String(actualLevel) + " (max: " + String(maxLevels - 1) + ")";
        log(levelMsg.c_str());
        
        String response = "{\"success\":true,\"level\":" + String(actualLevel) + ",\"maxLevels\":" + String(maxLevels) + "}";
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
            // Directly iterate through datalog directory to find all .bin files
            // This avoids the 50-session limit from listSessionFiles()
            File root = LittleFS.open("/datalog");
            if (root && root.isDirectory()) {
                File file = root.openNextFile();
                while (file) {
                    String fileName = String(file.name());
                    if (!file.isDirectory() && fileName.endsWith(".bin")) {
                        String fullPath = "/datalog/" + fileName;
                        file.close(); // Close file handle before deletion
                        
                        if (LittleFS.exists(fullPath)) {
                            if (LittleFS.remove(fullPath)) {
                                deleteCount++;
                                String delMsg = "Deleted session file: " + fullPath;
                                log(delMsg.c_str());
                            } else {
                                errorMsg += "Failed to delete " + fileName + "; ";
                                success = false;
                            }
                        } else {
                            errorMsg += "File not found " + fileName + "; ";
                        }
                    } else {
                        file.close(); // Close non-.bin files
                    }
                    file = root.openNextFile();
                }
                root.close();
            } else {
                errorMsg = "Could not open /datalog directory";
                success = false;
            }
            
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
        
    } else if (path == "/info.html") {
        // Serve info page
        if (loadFromSPIFFS(client, "/info.html")) {
            log("Served info.html from SPIFFS");
        } else {
            sendHttpResponse(client, 404, "text/plain", "Info page not found");
        }
        
    } else if (path == "/chart.min.js") {
        // Serve Chart.js library with enhanced fallback
        log("Serving Chart.js fallback");
        String chartJs = R"js(
window.Chart = class {
    constructor(ctx, config) {
        this.ctx = ctx;
        this.config = config;
        this.data = config.data || { labels: [], datasets: [] };
        this.canvas = ctx.canvas;
        this.canvas.style.backgroundColor = '#1e1e1e';
        this.canvas.width = 800;
        this.canvas.height = 400;
        this.update();
    }
    
    update() {
        const ctx = this.ctx;
        const canvas = this.canvas;
        
        // Clear canvas
        ctx.clearRect(0, 0, canvas.width, canvas.height);
        ctx.fillStyle = '#2a2a2a';
        ctx.fillRect(0, 0, canvas.width, canvas.height);
        
        // Check if we have valid data
        if (!this.data.datasets || this.data.datasets.length === 0 || !this.data.labels || this.data.labels.length === 0) {
            ctx.fillStyle = '#888';
            ctx.font = '16px Arial';
            ctx.textAlign = 'center';
            ctx.fillText('Chart.js not loaded - using fallback', canvas.width / 2, canvas.height / 2 - 20);
            ctx.fillText('Data points: 0', canvas.width / 2, canvas.height / 2 + 20);
            return;
        }
        
        const margin = 60;
        const chartWidth = canvas.width - 2 * margin;
        const chartHeight = canvas.height - 2 * margin;
        
        // Draw axes
        ctx.strokeStyle = '#555';
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.moveTo(margin, margin);
        ctx.lineTo(margin, canvas.height - margin);
        ctx.lineTo(canvas.width - margin, canvas.height - margin);
        ctx.stroke();
        
        // Colors for different datasets
        const colors = [
            '#4bc0c0', '#ff6384', '#ffce56', '#36a2eb', 
            '#9966ff', '#ff9f40', '#c7c7c7', '#ff63ff',
            '#63ff84', '#ffce84'
        ];
        
        // Find global min/max for all visible datasets
        let globalMin = Infinity;
        let globalMax = -Infinity;
        
        this.data.datasets.forEach(dataset => {
            if (dataset.data && dataset.data.length > 0) {
                const values = dataset.data.map(d => typeof d === 'object' ? d.y : d);
                const min = Math.min(...values);
                const max = Math.max(...values);
                if (min < globalMin) globalMin = min;
                if (max > globalMax) globalMax = max;
            }
        });
        
        const range = globalMax - globalMin || 1;
        
        // Draw datasets
        this.data.datasets.forEach((dataset, datasetIndex) => {
            if (!dataset.data || dataset.data.length === 0) return;
            
            const color = colors[datasetIndex % colors.length];
            ctx.strokeStyle = color;
            ctx.lineWidth = 2;
            ctx.beginPath();
            
            let hasValidPoint = false;
            for (let i = 0; i < dataset.data.length; i++) {
                const x = margin + (i / (dataset.data.length - 1)) * chartWidth;
                const val = typeof dataset.data[i] === 'object' ? dataset.data[i].y : dataset.data[i];
                const y = margin + chartHeight - ((val - globalMin) / range) * chartHeight;
                
                if (i === 0 || !hasValidPoint) {
                    ctx.moveTo(x, y);
                    hasValidPoint = true;
                } else {
                    ctx.lineTo(x, y);
                }
            }
            ctx.stroke();
        });
        
        // Draw legend
        ctx.font = '12px Arial';
        ctx.textAlign = 'left';
        let legendY = 20;
        this.data.datasets.forEach((dataset, index) => {
            if (dataset.label) {
                const color = colors[index % colors.length];
                ctx.fillStyle = color;
                ctx.fillRect(10, legendY - 8, 15, 10);
                ctx.fillStyle = '#ccc';
                ctx.fillText(dataset.label, 30, legendY);
                legendY += 15;
            }
        });
        
        // Draw title  
        ctx.fillStyle = '#ccc';
        ctx.font = '16px Arial';
        ctx.textAlign = 'center';
        ctx.fillText('DPV Data Visualization', canvas.width / 2, 20);
        
        // Draw data point count
        ctx.font = '12px Arial';
        ctx.fillText(`${this.data.labels.length} data points`, canvas.width / 2, canvas.height - 10);
        
        // Draw Y-axis labels
        ctx.font = '10px Arial';
        ctx.textAlign = 'right';
        ctx.fillStyle = '#888';
        for (let i = 0; i <= 5; i++) {
            const y = margin + (i / 5) * chartHeight;
            const value = globalMax - (i / 5) * range;
            ctx.fillText(value.toFixed(1), margin - 5, y + 3);
        }
        
        // Draw X-axis labels (time)
        ctx.textAlign = 'center';
        if (this.data.labels.length > 0) {
            const labelStep = Math.max(1, Math.floor(this.data.labels.length / 5));
            for (let i = 0; i < this.data.labels.length; i += labelStep) {
                const x = margin + (i / (this.data.labels.length - 1)) * chartWidth;
                ctx.fillText(this.data.labels[i], x, canvas.height - margin + 15);
            }
        }
    }
    
    destroy() {}
};
console.log('Chart.js fallback loaded');
)js";
        sendHttpResponse(client, 200, "application/javascript", chartJs.c_str());
        
    } else if (path == "/jszip.min.js") {
        // Serve JSZip library with enhanced fallback
        log("Serving JSZip fallback");
        String jszipJs = R"js(
window.JSZip = function() {
    return {
        files: {},
        file: function(name, content) {
            if (content !== undefined) {
                this.files[name] = content;
                return this;
            }
            return this.files[name];
        },
        generateAsync: function(options) {
            // Create a simple CSV export instead of ZIP
            let csvContent = '';
            let fileCount = 0;
            
            for (let filename in this.files) {
                fileCount++;
                csvContent += '=== ' + filename + ' ===\r\n';
                csvContent += this.files[filename];
                csvContent += '\r\n\r\n';
            }
            
            if (fileCount === 0) {
                csvContent = 'No data available for export';
            }
            
            // Return a proper Blob
            const blob = new Blob([csvContent], { type: 'text/plain;charset=utf-8' });
            return Promise.resolve(blob);
        }
    };
};
console.log('JSZip fallback loaded');
)js";
        sendHttpResponse(client, 200, "application/javascript", jszipJs.c_str());
        
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
        
        // Update beeper setting in both old and new systems
        currentSettings.beeperEnabled = newBeeperState;
        saveSettings(); // Save to unified settings system
        
        String response = "{\"success\":true,\"enabled\":" + String(getBeeperEnabled() ? "true" : "false") + "}";
        sendHttpResponse(client, 200, "application/json", response.c_str());
        
        String beeperMsg = "Beeper setting updated: " + String(getBeeperEnabled() ? "enabled" : "disabled");
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
    WiFi.softAP(getWifiSSID(), getWifiPassword());
    
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
