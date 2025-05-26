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
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
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
            
            // Set up periodic updates
            setInterval(loadData, updateInterval);
        });
        
        // Tab Navigation
        function showTab(tabName) {
            // Hide all tabs
            document.querySelectorAll('.tab-content').forEach(tab => {
                tab.style.display = 'none';
            });
            
            // Show selected tab
            document.getElementById(tabName + '-tab').style.display = 'block';
            
            // Update active state of buttons
            document.querySelectorAll('.nav-tab').forEach(btn => {
                btn.classList.remove('active');
            });
            
            // Find the button that was clicked
            event.target.classList.add('active');
        }
        
        // Save settings
        function saveSettings() {
            updateInterval = parseInt(document.getElementById('updateInterval').value) * 1000;
            dataPointsToShow = parseInt(document.getElementById('dataPoints').value);
            
            localStorage.setItem('updateInterval', updateInterval / 1000);
            localStorage.setItem('dataPoints', dataPointsToShow);
            
            alert('Settings saved!');
        }
        
        // Initialize Charts
        function initCharts() {
            // Temperature Chart
            const tempCtx = document.getElementById('tempChart').getContext('2d');
            charts.tempChart = new Chart(tempCtx, {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [{
                        label: 'Motor Temperature (°C)',
                        backgroundColor: 'rgba(255, 99, 132, 0.2)',
                        borderColor: 'rgba(255, 99, 132, 1)',
                        data: []
                    }, {
                        label: 'Ambient Temperature (°C)',
                        backgroundColor: 'rgba(54, 162, 235, 0.2)',
                        borderColor: 'rgba(54, 162, 235, 1)',
                        data: []
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false
                }
            });
            
            // Battery Chart
            const batteryCtx = document.getElementById('batteryChart').getContext('2d');
            charts.batteryChart = new Chart(batteryCtx, {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [{
                        label: 'Battery Voltage (V)',
                        yAxisID: 'y',
                        backgroundColor: 'rgba(75, 192, 192, 0.2)',
                        borderColor: 'rgba(75, 192, 192, 1)',
                        data: []
                    }, {
                        label: 'Current (A)',
                        yAxisID: 'y1',
                        backgroundColor: 'rgba(153, 102, 255, 0.2)',
                        borderColor: 'rgba(153, 102, 255, 1)',
                        data: []
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    scales: {
                        y: {
                            type: 'linear',
                            position: 'left',
                        },
                        y1: {
                            type: 'linear',
                            position: 'right',
                            grid: {
                                drawOnChartArea: false
                            }
                        }
                    }
                }
            });
            
            // Motor Chart
            const motorCtx = document.getElementById('motorChart').getContext('2d');
            charts.motorChart = new Chart(motorCtx, {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [{
                        label: 'RPM',
                        yAxisID: 'y',
                        backgroundColor: 'rgba(255, 206, 86, 0.2)',
                        borderColor: 'rgba(255, 206, 86, 1)',
                        data: []
                    }, {
                        label: 'Duty Cycle (%)',
                        yAxisID: 'y1',
                        backgroundColor: 'rgba(75, 192, 192, 0.2)',
                        borderColor: 'rgba(75, 192, 192, 1)',
                        data: []
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    scales: {
                        y: {
                            type: 'linear',
                            position: 'left',
                        },
                        y1: {
                            type: 'linear',
                            position: 'right',
                            grid: {
                                drawOnChartArea: false
                            }
                        }
                    }
                }
            });
        }
        
        // Load data from the API
        function loadData() {
            // Fetch status data
            fetch('/api/status')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('uptime').textContent = formatTime(data.uptime);
                })
                .catch(error => console.error('Error fetching status:', error));
            
            // Fetch chart data
            fetch('/api/data?count=' + dataPointsToShow)
                .then(response => response.json())
                .then(data => {
                    if (data.length === 0) return;
                    
                    // Update status values with latest data
                    const latest = data[data.length - 1];
                    document.getElementById('battery').textContent = latest.batteryVoltage.toFixed(2) + ' V';
                    document.getElementById('temperature').textContent = latest.temperature.toFixed(1) + ' °C';
                    document.getElementById('humidity').textContent = latest.humidity.toFixed(1) + ' %';
                    
                    // Update charts
                    updateCharts(data);
                })
                .catch(error => console.error('Error fetching data:', error));
        }
        
        // Update charts with new data
        function updateCharts(data) {
            // Get timestamps for x-axis
            const labels = data.map(item => {
                const date = new Date(item.timestamp);
                return date.toLocaleTimeString();
            });
            
            // Update temperature chart
            charts.tempChart.data.labels = labels;
            charts.tempChart.data.datasets[0].data = data.map(item => item.tempMotor);
            charts.tempChart.data.datasets[1].data = data.map(item => item.temperature);
            charts.tempChart.update();
            
            // Update battery chart
            charts.batteryChart.data.labels = labels;
            charts.batteryChart.data.datasets[0].data = data.map(item => item.batteryVoltage);
            charts.batteryChart.data.datasets[1].data = data.map(item => item.current);
            charts.batteryChart.update();
            
            // Update motor chart
            charts.motorChart.data.labels = labels;
            charts.motorChart.data.datasets[0].data = data.map(item => item.rpm);
            charts.motorChart.data.datasets[1].data = data.map(item => item.dutyCycle);
            charts.motorChart.update();
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