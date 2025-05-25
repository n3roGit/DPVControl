#include "data_upload.h"
#include "log.h"

// Initialize file system and store HTML files
bool initializeFileSystem() {
    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        log("An error occurred while mounting SPIFFS");
        return false;
    }
    
    log("SPIFFS mounted successfully");
    
    // Prüfe verfügbaren Platz
    size_t totalBytes = SPIFFS.totalBytes();
    size_t usedBytes = SPIFFS.usedBytes();
    log("SPIFFS Total: ", totalBytes);
    log("SPIFFS Used: ", usedBytes);
    
    // Kleines Delay für Hardware-Operationen
    delay(100);
    
    // Store index.html in SPIFFS if it doesn't exist
    if (!SPIFFS.exists("/index.html")) {
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
                        <td><input type="number" id="dataPoints" min="10" max="3600" value="60"></td>
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
        if (totalBytes - usedBytes < strlen(indexHTML) + 1024) { // 1KB Puffer für sichere Schätzung
            log("Nicht genug Platz im Dateisystem für index.html");
            return false;
        }

        // Datei in kleineren Blöcken schreiben
        File file = SPIFFS.open("/index.html", "w");
        if (!file) {
            log("Failed to open file for writing");
            return false;
        }
        
        // Schreibe die Datei in Blöcken zu 1024 Bytes
        const size_t chunkSize = 1024;
        size_t remaining = strlen(indexHTML);
        size_t position = 0;
        
        while (remaining > 0) {
            size_t toWrite = remaining > chunkSize ? chunkSize : remaining;
            
            if (!file.write((const uint8_t*)(indexHTML + position), toWrite)) {
                log("Failed to write to file");
                file.close();
                return false;
            }
            
            position += toWrite;
            remaining -= toWrite;
            
            // Kurze Pause nach jedem Block
            delay(5);
        }
        
        file.flush();
        file.close();
        
        log("index.html stored in SPIFFS");
    }
    
    return true;
}

// Store a file in SPIFFS
bool storeFile(const char* path, const char* content) {
    File file = SPIFFS.open(path, "w");
    if (!file) {
        log("Failed to open file for writing");
        return false;
    }
    
    // Datei in Blöcken schreiben
    const size_t chunkSize = 1024;
    size_t remaining = strlen(content);
    size_t position = 0;
    
    while (remaining > 0) {
        size_t toWrite = remaining > chunkSize ? chunkSize : remaining;
        
        if (!file.write((const uint8_t*)(content + position), toWrite)) {
            log("Failed to write to file");
            file.close();
            return false;
        }
        
        position += toWrite;
        remaining -= toWrite;
        
        // Kurze Pause nach jedem Block
        delay(5);
    }
    
    file.flush();
    file.close();
    return true;
} 