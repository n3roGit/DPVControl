#include "data_upload.h"
#include "log.h"
#include <LittleFS.h>

// Initialize file system and store HTML files
bool initializeFileSystem() {
    // Initialize LittleFS
    if (!LittleFS.begin(true)) {
        log("An error occurred while mounting LittleFS");
        return false;
    }
    
    log("LittleFS mounted successfully");
    
    // Prüfe verfügbaren Platz
    size_t totalBytes = LittleFS.totalBytes();
    size_t usedBytes = LittleFS.usedBytes();
    String totalMsg = "LittleFS Total: " + String(totalBytes);
    log(totalMsg.c_str());
    String usedMsg = "LittleFS Used: " + String(usedBytes);
    log(usedMsg.c_str());
    
    // Längeres Delay für Hardware-Operationen
    delay(200);
    
    // Store index.html in LittleFS if it doesn't exist
    if (!LittleFS.exists("/index.html")) {
        log("index.html nicht gefunden, erstelle neu");
        
        // Überprüfen, ob genug Platz für die Datei vorhanden ist
        size_t freeBytes = totalBytes - usedBytes;
        String freeMsg = "LittleFS Free: " + String(freeBytes);
        log(freeMsg.c_str());
        
        // Content of index.html - this should be updated with the actual content
        const char* indexHTML = R"rawliteral(
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
        }
        .button:hover {
            background-color: #2c3e50;
        }
        .chart-container {
            width: 100%;
            height: 400px;
            margin: 20px 0;
        }
        .nav-tab {
            padding: 10px 20px;
            background-color: #f8f8f8;
            border: none;
            cursor: pointer;
            transition: background-color 0.3s ease;
        }
        .nav-tab.active {
            background-color: #3498db;
            color: white;
        }
    </style>
                <!-- Local Chart.js for offline functionality -->
            <script>
                // Load Chart.js from local SPIFFS for data upload page
                fetch('/chart.min.js')
                    .then(response => {
                        if (!response.ok) throw new Error('Chart.js not found');
                        return response.text();
                    })
                    .then(script => {
                        const scriptElement = document.createElement('script');
                        scriptElement.textContent = script;
                        document.head.appendChild(scriptElement);
                        console.log('Chart.js loaded successfully for data upload page');
                    })
                    .catch(error => {
                        console.warn('Chart.js not available locally for data upload, using fallback:', error);
                        // Fallback: Simple chart placeholder for data upload page
                        window.Chart = class {
                            constructor(ctx, config) {
                                this.ctx = ctx;
                                this.config = config;
                                this.data = config.data || { labels: [], datasets: [] };
                                this.canvas = ctx.canvas;
                                this.canvas.style.backgroundColor = '#1e1e1e';
                                this.canvas.width = 600;
                                this.canvas.height = 300;
                                this.update();
                            }
                            
                            update() {
                                const ctx = this.ctx;
                                ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
                                ctx.fillStyle = '#333';
                                ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
                                ctx.fillStyle = '#4fc3f7';
                                ctx.font = '14px Arial';
                                ctx.textAlign = 'center';
                                ctx.fillText('Chart.js offline mode - Data upload page', this.canvas.width / 2, this.canvas.height / 2);
                            }
                            
                            destroy() {}
                        };
                    });
            </script>
</head>
<body>
    <div class="container">
        <h1>DPVControl Web Interface</h1>
        
        <div class="section">
            <div class="tab-navigation">
                <button class="nav-tab active" onclick="showTab('status')">Status</button>
                <button class="nav-tab" onclick="showTab('charts')">Charts</button>
                <button class="nav-tab" onclick="showTab('settings')">Settings</button>
            </div>
        </div>
        
        <div id="status-tab" class="tab-content">
            <div class="section">
                <h2>System Status</h2>
                <table>
                    <tr>
                        <td>Uptime:</td>
                        <td id="uptime">Loading...</td>
                    </tr>
                    <tr>
                        <td>Battery Voltage:</td>
                        <td id="battery">Loading...</td>
                    </tr>
                    <tr>
                        <td>Temperature:</td>
                        <td id="temperature">Loading...</td>
                    </tr>
                    <tr>
                        <td>Humidity:</td>
                        <td id="humidity">Loading...</td>
                    </tr>
                </table>
            </div>
            
            <div class="section">
                <h2>Controls</h2>
                <button class="button" id="led-toggle">Toggle LED</button>
            </div>
        </div>
        
        <div id="charts-tab" class="tab-content" style="display:none;">
            <div class="section">
                <h2>Temperature Data</h2>
                <div class="chart-container">
                    <canvas id="tempChart"></canvas>
                </div>
            </div>
            
            <div class="section">
                <h2>Battery and Current</h2>
                <div class="chart-container">
                    <canvas id="batteryChart"></canvas>
                </div>
            </div>
            
            <div class="section">
                <h2>Motor Data</h2>
                <div class="chart-container">
                    <canvas id="motorChart"></canvas>
                </div>
            </div>
        </div>
        
        <div id="settings-tab" class="tab-content" style="display:none;">
            <div class="section">
                <h2>Settings</h2>
                <table>
                    <tr>
                        <td>Update Interval (s):</td>
                        <td><input type="number" id="updateInterval" min="1" max="60" value="5"></td>
                    </tr>
                    <tr>
                        <td>Data Points to Show:</td>
                        <td><input type="number" id="dataPoints" min="10" max="600" value="60"></td>
                    </tr>
                </table>
                <button class="button" onclick="saveSettings()">Save Settings</button>
            </div>
        </div>
    </div>

    <script>
        // Variables
        let updateInterval = 5000; // 5 seconds
        let dataPointsToShow = 60;
        let charts = {};
        let isConnected = true;
        let updateTimer = null;
        
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
            
            // Initialize charts
            initCharts();
            
            // First data load
            loadData();
            
            // Set up periodic updates with connection check
            startUpdateCycle();
            
            // Add visibility change handler
            document.addEventListener('visibilitychange', handleVisibilityChange);
        });
        
        // Handle page visibility changes
        function handleVisibilityChange() {
            if (document.hidden) {
                stopUpdateCycle();
            } else {
                startUpdateCycle();
            }
        }
        
        // Start the update cycle
        function startUpdateCycle() {
            if (updateTimer) {
                clearInterval(updateTimer);
            }
            loadData(); // Initial load
            updateTimer = setInterval(() => {
                if (isConnected) {
                    loadData();
                } else {
                    reconnect();
                }
            }, updateInterval);
        }
        
        // Stop the update cycle
        function stopUpdateCycle() {
            if (updateTimer) {
                clearInterval(updateTimer);
                updateTimer = null;
            }
        }
        
        // Attempt to reconnect
        async function reconnect() {
            console.log('Attempting to reconnect...');
            try {
                const response = await fetch('/api/status', { timeout: 2000 });
                if (response.ok) {
                    console.log('Reconnected successfully');
                    isConnected = true;
                    loadData();
                }
            } catch (error) {
                console.log('Reconnection failed, will retry in ' + (updateInterval/1000) + ' seconds');
                isConnected = false;
            }
        }
        
        // Save settings with connection management
        function saveSettings() {
            const newInterval = parseInt(document.getElementById('updateInterval').value) * 1000;
            const newDataPoints = parseInt(document.getElementById('dataPoints').value);
            
            if (newInterval !== updateInterval || newDataPoints !== dataPointsToShow) {
                updateInterval = newInterval;
                dataPointsToShow = newDataPoints;
                
                localStorage.setItem('updateInterval', updateInterval / 1000);
                localStorage.setItem('dataPoints', dataPointsToShow);
                
                // Restart update cycle with new interval
                startUpdateCycle();
                
                alert('Settings saved! Update cycle restarted.');
            }
        }
        
        // Load data from the API with connection management
        async function loadData(retryCount = 0) {
            const maxRetries = 3;
            const retryDelay = 1000; // 1 second
            
            if (!navigator.onLine) {
                console.log('Browser is offline');
                isConnected = false;
                return;
            }
            
            try {
                const controller = new AbortController();
                const timeoutId = setTimeout(() => controller.abort(), 5000);
                
                // Fetch status data with timeout
                const statusResponse = await fetch('/api/status', {
                    signal: controller.signal
                });
                
                clearTimeout(timeoutId);
                
                if (!statusResponse.ok) throw new Error('Status API error');
                const data = await statusResponse.json();
                
                isConnected = true; // Mark as connected on successful response
                
                requestAnimationFrame(() => {
                    document.getElementById('uptime').textContent = formatTime(data.uptime);
                });
            } catch (error) {
                console.error('Error fetching status:', error);
                if (error.name === 'AbortError') {
                    console.log('Request timed out');
                }
                isConnected = false;
                
                if (retryCount < maxRetries) {
                    console.log(`Retrying status fetch in ${retryDelay}ms... (${retryCount + 1}/${maxRetries})`);
                    await new Promise(resolve => setTimeout(resolve, retryDelay));
                    return loadData(retryCount + 1);
                }
                return; // Don't proceed with chart data if status failed
            }
            
            try {
                const controller = new AbortController();
                const timeoutId = setTimeout(() => controller.abort(), 5000);
                
                // Fetch chart data with timeout
                const dataResponse = await fetch('/api/data?count=' + dataPointsToShow, {
                    signal: controller.signal
                });
                
                clearTimeout(timeoutId);
                
                if (!dataResponse.ok) throw new Error('Data API error');
                const data = await dataResponse.json();
                
                if (data.length === 0) return;
                
                isConnected = true; // Mark as connected on successful response
                
                // Update status values with latest data
                const latest = data[data.length - 1];
                requestAnimationFrame(() => {
                    document.getElementById('battery').textContent = latest.batteryVoltage.toFixed(2) + ' V';
                    document.getElementById('temperature').textContent = latest.temperature.toFixed(1) + ' °C';
                    document.getElementById('humidity').textContent = latest.humidity.toFixed(1) + ' %';
                });
                
                // Update charts
                updateCharts(data);
            } catch (error) {
                console.error('Error fetching chart data:', error);
                if (error.name === 'AbortError') {
                    console.log('Request timed out');
                }
                isConnected = false;
                
                if (retryCount < maxRetries) {
                    console.log(`Retrying chart data fetch in ${retryDelay}ms... (${retryCount + 1}/${maxRetries})`);
                    await new Promise(resolve => setTimeout(resolve, retryDelay));
                    return loadData(retryCount + 1);
                }
            }
        }
        
        // Format time in HH:MM:SS
        function formatTime(milliseconds) {
            const totalSeconds = Math.floor(milliseconds / 1000);
            const hours = Math.floor(totalSeconds / 3600);
            const minutes = Math.floor((totalSeconds % 3600) / 60);
            const seconds = totalSeconds % 60;
            
            return `${String(hours).padStart(2, '0')}:${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`;
        }
        
        // Toggle LED (placeholder)
        document.getElementById('led-toggle').addEventListener('click', function() {
            alert('LED toggle functionality will be added in the future');
        });
    </script>
</body>
</html>
)rawliteral";

        // Überprüfen, ob genug Platz für die Datei vorhanden ist
        size_t htmlSize = strlen(indexHTML);
        if (freeBytes < htmlSize + 1024) { // 1KB Puffer für sichere Schätzung
            log("Nicht genug Platz im Dateisystem für index.html");
            String neededMsg = "Benötigt: " + String(htmlSize + 1024);
            log(neededMsg.c_str());
            String availableMsg = "Verfügbar: " + String(freeBytes);
            log(availableMsg.c_str());
            return false;
        }

        String startMsg = "Starte Schreiben von index.html (Größe: " + String(htmlSize) + " Bytes)";
        log(startMsg.c_str());
        
        // Versuche zuerst, mögliche alte Datei zu löschen
        if (LittleFS.exists("/index.html")) {
            LittleFS.remove("/index.html");
            delay(100);
        }
        
        // Datei in kleineren Blöcken schreiben
        File file = LittleFS.open("/index.html", "w");
        if (!file) {
            log("Failed to open file for writing");
            return false;
        }
        
        // Schreibe die Datei in Blöcken zu 512 Bytes (kleinere Blöcke)
        const size_t chunkSize = 512;
        size_t remaining = htmlSize;
        size_t position = 0;
        
        while (remaining > 0) {
            size_t toWrite = remaining > chunkSize ? chunkSize : remaining;
            
            if (!file.write((const uint8_t*)(indexHTML + position), toWrite)) {
                String failMsg = "Failed to write to file at position " + String(position);
                log(failMsg.c_str());
                file.close();
                return false;
            }
            
            position += toWrite;
            remaining -= toWrite;
            
            // Längere Pause nach jedem Block
            delay(20);
        }
        
        file.flush();
        delay(20);
        file.close();
        delay(20);
        
        // Überprüfen ob die Datei erfolgreich geschrieben wurde
        if (LittleFS.exists("/index.html")) {
            File checkFile = LittleFS.open("/index.html", "r");
            if (checkFile && checkFile.size() > 0) {
                String successMsg = "index.html erfolgreich gespeichert (" + String(checkFile.size()) + " Bytes)";
                log(successMsg.c_str());
                checkFile.close();
            } else {
                log("index.html existiert, aber ist möglicherweise leer");
                if (checkFile) checkFile.close();
                return false;
            }
        } else {
            log("index.html konnte nicht gespeichert werden");
            return false;
        }
    } else {
        log("index.html bereits vorhanden, überspringe");
    }
    
    // Store version.txt in LittleFS if it doesn't exist
    if (!LittleFS.exists("/version.txt")) {
        log("Creating version.txt file...");
        
        const char* versionContent = "2.0.0";
        
        if (!storeFile("/version.txt", versionContent)) {
            log("Failed to store version.txt, but continuing anyway");
        } else {
            log("version.txt stored successfully");
        }
    } else {
        log("version.txt already exists, skipping");
    }
    
    return true;
}

// Store a file in LittleFS
bool storeFile(const char* path, const char* content) {
    String logMsg = "Speichere Datei: " + String(path);
    log(logMsg.c_str());
    
    // Längeres Delay für Hardware-Operationen
    delay(100);
    
    // Versuche zuerst, mögliche alte Datei zu löschen
    if (LittleFS.exists(path)) {
        LittleFS.remove(path);
        delay(100);
    }
    
    File file = LittleFS.open(path, "w");
    if (!file) {
        String errorMsg = "Failed to open file for writing: " + String(path);
        log(errorMsg.c_str());
        return false;
    }
    
    // Datei in Blöcken schreiben
    const size_t chunkSize = 512; // Reduziert die Blockgröße
    size_t contentSize = strlen(content);
    size_t remaining = contentSize;
    size_t position = 0;
    
    String sizeMsg = "Schreibe " + String(contentSize) + " Bytes in Blöcken zu " + String(chunkSize) + " Bytes";
    log(sizeMsg.c_str());
    
    while (remaining > 0) {
        size_t toWrite = remaining > chunkSize ? chunkSize : remaining;
        
        if (!file.write((const uint8_t*)(content + position), toWrite)) {
            String failMsg = "Failed to write to file at position " + String(position);
            log(failMsg.c_str());
            file.close();
            return false;
        }
        
        position += toWrite;
        remaining -= toWrite;
        
        // Längere Pause nach jedem Block
        delay(20);
    }
    
    file.flush();
    delay(20);
    file.close();
    delay(20);
    
    // Überprüfen ob die Datei erfolgreich geschrieben wurde
    if (LittleFS.exists(path)) {
        File checkFile = LittleFS.open(path, "r");
        if (checkFile && checkFile.size() > 0) {
            String successMsg = "Datei erfolgreich gespeichert: " + String(path) + " (" + String(checkFile.size()) + " Bytes)";
            log(successMsg.c_str());
            checkFile.close();
            return true;
        } else {
            String emptyMsg = "Datei existiert, aber ist möglicherweise leer: " + String(path);
            log(emptyMsg.c_str());
            if (checkFile) checkFile.close();
            return false;
        }
    } else {
        String failMsg = "Datei konnte nicht gespeichert werden: " + String(path);
        log(failMsg.c_str());
        return false;
    }
} 