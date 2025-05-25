#include "webserver.h"
#include "log.h"
#include "data_upload.h"
#include "datalog.h"  // Einbinden des Datalogger-Headers

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
            background-color: #e0e5e9;
            color: #1e272e;
        }
        h1, h2 {
            color: #3498db;
        }
        .container {
            max-width: 800px;
            margin: 20px auto;
            padding: 20px;
            background-color: white;
            border-radius: 5px;
            box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
        }
        .section {
            margin-bottom: 20px;
            padding: 15px;
            border-bottom: 1px solid #eee;
        }
        table {
            width: 100%;
            border-collapse: collapse;
        }
        th, td {
            padding: 8px;
            text-align: left;
            border-bottom: 1px solid #ddd;
        }
        .button {
            background-color: #3498db;
            color: white;
            border: none;
            padding: 10px 15px;
            border-radius: 4px;
            cursor: pointer;
            margin: 5px;
        }
        .button:hover {
            background-color: #2c3e50;
        }
        .nav-tab {
            padding: 10px 20px;
            background-color: #f8f8f8;
            border: none;
            cursor: pointer;
            transition: background-color 0.3s ease;
            margin-right: 5px;
        }
        .nav-tab.active {
            background-color: #3498db;
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
            color: #2c3e50;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>DPVControl Web Interface</h1>
        
        <div class="section">
            <div class="tab-navigation">
                <button class="nav-tab active" onclick="showTab('status')">Status</button>
                <button class="nav-tab" onclick="showTab('data')">Data</button>
                <button class="nav-tab" onclick="showTab('settings')">Settings</button>
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
                        <td>Battery Voltage:</td>
                        <td class="status-value" id="battery">Loading...</td>
                    </tr>
                    <tr>
                        <td>Motor Temperature:</td>
                        <td class="status-value" id="motorTemp">Loading...</td>
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
                        <td>Current:</td>
                        <td class="status-value" id="current">Loading...</td>
                    </tr>
                    <tr>
                        <td>RPM:</td>
                        <td class="status-value" id="rpm">Loading...</td>
                    </tr>
                    <tr>
                        <td>Duty Cycle:</td>
                        <td class="status-value" id="dutyCycle">Loading...</td>
                    </tr>
                </table>
            </div>
        </div>
        
        <div id="data-tab" class="tab-content">
            <div class="section">
                <h2>Recent Data Points</h2>
                <p>Data Points Available: <span id="dataPointCount">Loading...</span></p>
                <button class="button" onclick="loadRecentData()">Refresh Data</button>
                <div id="dataDisplay" style="margin-top: 20px;">
                    <p>Click "Refresh Data" to load recent measurements...</p>
                </div>
            </div>
        </div>
        
        <div id="settings-tab" class="tab-content">
            <div class="section">
                <h2>Settings</h2>
                <table>
                    <tr>
                        <td>Update Interval (s):</td>
                        <td><input type="number" id="updateInterval" min="1" max="60" value="5"></td>
                    </tr>
                    <tr>
                        <td>Data Points to Show:</td>
                        <td><input type="number" id="dataPoints" min="10" max="100" value="20"></td>
                    </tr>
                </table>
                <button class="button" onclick="saveSettings()">Save Settings</button>
            </div>
        </div>
    </div>

    <script>
        // Variables
        let updateInterval = 5000; // 5 seconds
        let dataPointsToShow = 20;
        
        // Initialize the application
        document.addEventListener('DOMContentLoaded', function() {
            // Load settings from localStorage
            if (localStorage.getItem('updateInterval')) {
                updateInterval = parseInt(localStorage.getItem('updateInterval')) * 1000;
                document.getElementById('updateInterval').value = updateInterval / 1000;
            }
            
            if (localStorage.getItem('dataPoints')) {
                dataPointsToShow = parseInt(localStorage.getItem('dataPoints'));
                document.getElementById('dataPoints').value = dataPointsToShow;
            }
            
            // First data load
            loadData();
            
            // Set up periodic updates
            setInterval(loadData, updateInterval);
        });
        
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
        
        // Save settings
        function saveSettings() {
            updateInterval = parseInt(document.getElementById('updateInterval').value) * 1000;
            dataPointsToShow = parseInt(document.getElementById('dataPoints').value);
            
            localStorage.setItem('updateInterval', updateInterval / 1000);
            localStorage.setItem('dataPoints', dataPointsToShow);
            
            alert('Settings saved! Page will reload to apply changes.');
            location.reload();
        }
        
        // Load data from the API
        function loadData() {
            // Fetch status data
            fetch('/api/status')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('uptime').textContent = formatTime(data.uptime);
                    document.getElementById('dataPointCount').textContent = data.dataPoints || 0;
                })
                .catch(error => {
                    console.error('Error fetching status:', error);
                    document.getElementById('uptime').textContent = 'Error loading';
                });
            
            // Fetch latest data point for status display
            fetch('/api/data?count=1')
                .then(response => response.json())
                .then(data => {
                    if (data.length > 0) {
                        const latest = data[0];
                        document.getElementById('battery').textContent = latest.batteryVoltage.toFixed(2) + ' V';
                        document.getElementById('motorTemp').textContent = latest.tempMotor.toFixed(1) + ' °C';
                        document.getElementById('temperature').textContent = latest.temperature.toFixed(1) + ' °C';
                        document.getElementById('humidity').textContent = latest.humidity.toFixed(1) + ' %';
                        document.getElementById('current').textContent = latest.current.toFixed(2) + ' A';
                        document.getElementById('rpm').textContent = latest.rpm.toFixed(0) + ' RPM';
                        document.getElementById('dutyCycle').textContent = latest.dutyCycle.toFixed(1) + ' %';
                    }
                })
                .catch(error => {
                    console.error('Error fetching data:', error);
                    document.getElementById('battery').textContent = 'Error loading';
                });
        }
        
        // Load recent data for the data tab
        function loadRecentData() {
            fetch('/api/data?count=' + dataPointsToShow)
                .then(response => response.json())
                .then(data => {
                    let html = '<table><tr><th>Time</th><th>Battery (V)</th><th>Motor Temp (°C)</th><th>Current (A)</th><th>RPM</th><th>Duty (%)</th></tr>';
                    
                    data.forEach(item => {
                        const date = new Date(item.timestamp);
                        const timeStr = date.toLocaleTimeString();
                        html += '<tr>';
                        html += '<td>' + timeStr + '</td>';
                        html += '<td>' + item.batteryVoltage.toFixed(2) + '</td>';
                        html += '<td>' + item.tempMotor.toFixed(1) + '</td>';
                        html += '<td>' + item.current.toFixed(2) + '</td>';
                        html += '<td>' + item.rpm.toFixed(0) + '</td>';
                        html += '<td>' + item.dutyCycle.toFixed(1) + '</td>';
                        html += '</tr>';
                    });
                    
                    html += '</table>';
                    document.getElementById('dataDisplay').innerHTML = html;
                })
                .catch(error => {
                    console.error('Error fetching data:', error);
                    document.getElementById('dataDisplay').innerHTML = '<p>Error loading data</p>';
                });
        }
        
        // Format time in HH:MM:SS
        function formatTime(milliseconds) {
            const totalSeconds = Math.floor(milliseconds / 1000);
            const hours = Math.floor(totalSeconds / 3600);
            const minutes = Math.floor((totalSeconds % 3600) / 60);
            const seconds = totalSeconds % 60;
            
            return String(hours).padStart(2, '0') + ':' + String(minutes).padStart(2, '0') + ':' + String(seconds).padStart(2, '0');
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
    File dataFile = SPIFFS.open(path.c_str(), "r");
    
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
String generateDataLoggerJson(int count) {
    LogdataRow* dataPoints = getLatestDataPoints(count);
    if (!dataPoints) {
        return "[]";
    }
    
    String json = "[";
    for (int i = 0; i < count && i < totalDataPoints; i++) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"timestamp\":" + String(dataPoints[i].timestamp) + ",";
        json += "\"tempMotor\":" + String(dataPoints[i].tempMotor) + ",";
        json += "\"batteryVoltage\":" + String(dataPoints[i].batteryVoltage) + ",";
        json += "\"current\":" + String(dataPoints[i].current) + ",";
        json += "\"rpm\":" + String(dataPoints[i].rpm) + ",";
        json += "\"dutyCycle\":" + String(dataPoints[i].dutyCycle) + ",";
        json += "\"temperature\":" + String(dataPoints[i].temperature) + ",";
        json += "\"humidity\":" + String(dataPoints[i].humidity);
        json += "}";
    }
    json += "]";
    return json;
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
    
    // Read the first line of the request
    String request = client.readStringUntil('\r');
    client.readStringUntil('\n');
    
    // Extract the request method and path
    int firstSpace = request.indexOf(' ');
    int secondSpace = request.indexOf(' ', firstSpace + 1);
    
    if (firstSpace == -1 || secondSpace == -1) {
        client.stop();
        return;
    }
    
    String method = request.substring(0, firstSpace);
    String path = request.substring(firstSpace + 1, secondSpace);
    String host = "";
    
    log(("Request: " + method + " " + path).c_str());
    
    // Get the host from headers - important for captive portal detection
    while (client.connected() && client.available()) {
        String line = client.readStringUntil('\n');
        line.trim();
        
        if (line.startsWith("Host: ")) {
            host = line.substring(6);
            log(("Host: " + host).c_str());
        }
        
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
        int count = 60; // Default: return 60 data points
        
        // Extract count parameter if present
        if (path.indexOf("count=") != -1) {
            String countStr = path.substring(path.indexOf("count=") + 6);
            if (countStr.indexOf("&") != -1) {
                countStr = countStr.substring(0, countStr.indexOf("&"));
            }
            count = countStr.toInt();
            if (count <= 0 || count > MAX_DATA_POINTS) {
                count = 60; // Fallback to default
            }
        }
        
        // Generate and send JSON data
        String jsonData = generateDataLoggerJson(count);
        sendHttpResponse(client, 200, "application/json", jsonData.c_str());
        
    } else if (path == "/api/status") {
        // API endpoint for system status
        String jsonStatus = "{";
        jsonStatus += "\"uptime\":" + String(millis()) + ",";
        jsonStatus += "\"dataPoints\":" + String(totalDataPoints);
        jsonStatus += "}";
        sendHttpResponse(client, 200, "application/json", jsonStatus.c_str());
        
    } else if (path == "/generate_204" || path == "/ncsi.txt" || 
               path == "/connecttest.txt" || path == "/redirect" || 
               path == "/hotspot-detect.html" || path.indexOf("success.txt") != -1 || 
               path.indexOf("success.html") != -1) {
        
        // Android/Windows/iOS captive portal detection
        log("Captive portal check detected");
        sendHttpResponse(client, 302, "text/html", "<html><head><meta http-equiv='refresh' content='0; URL=http://4.3.2.1/'></head><body>Redirecting...</body></html>");
    
    } else if (spiffsInitialized && SPIFFS.exists(path)) {
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