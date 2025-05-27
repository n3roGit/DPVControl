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
    
    // Check available space
    size_t totalBytes = LittleFS.totalBytes();
    size_t usedBytes = LittleFS.usedBytes();
    String totalMsg = "LittleFS Total: " + String(totalBytes);
    log(totalMsg.c_str());
    String usedMsg = "LittleFS Used: " + String(usedBytes);
    log(usedMsg.c_str());
    
    // Longer delay for hardware operations
    delay(200);
    
    // Store index.html in LittleFS if it doesn't exist
    if (!LittleFS.exists("/index.html")) {
        log("index.html not found, creating new one");
        
        // Check if enough space is available for the file
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
        .logo {
            max-width: 200px;
            margin: 20px auto;
            display: block;
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
                                this.canvas.style.backgroundColor = '#f8f8f8';
                                this.canvas.width = 600;
                                this.canvas.height = 300;
                                this.update();
                            }
                            
                            update() {
                                const ctx = this.ctx;
                                ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
                                ctx.fillStyle = '#f8f8f8';
                                ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
                                ctx.fillStyle = '#666';
                                ctx.font = '14px Arial';
                                ctx.textAlign = 'center';
                                ctx.fillText('Charts disabled - Chart.js not available', this.canvas.width / 2, this.canvas.height / 2);
                            }
                            
                            destroy() {}
                        };
                        
                        // Simple JSZip placeholder for export functionality
                        window.JSZip = class {
                            constructor() {
                                this.files = {};
                            }
                            
                            file(name, content) {
                                this.files[name] = content;
                            }
                            
                            generateAsync() {
                                return Promise.reject(new Error('JSZip library not loaded. Please refresh the page.'));
                            }
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
        
        // Initialize charts function
        function initCharts() {
            if (typeof Chart === 'undefined') {
                console.log('Chart.js not loaded, charts disabled');
                return;
            }
            
            // Initialize temperature chart
            const tempCtx = document.getElementById('tempChart');
            if (tempCtx) {
                charts.tempChart = new Chart(tempCtx, {
                    type: 'line',
                    data: {
                        labels: [],
                        datasets: [{
                            label: 'Temperature °C',
                            data: [],
                            borderColor: '#3498db',
                            fill: false
                        }]
                    },
                    options: {
                        responsive: true,
                        scales: {
                            y: {
                                beginAtZero: false
                            }
                        }
                    }
                });
            }
            
            // Initialize battery chart
            const batteryCtx = document.getElementById('batteryChart');
            if (batteryCtx) {
                charts.batteryChart = new Chart(batteryCtx, {
                    type: 'line',
                    data: {
                        labels: [],
                        datasets: [{
                            label: 'Battery Voltage',
                            data: [],
                            borderColor: '#e74c3c',
                            fill: false
                        }, {
                            label: 'Current',
                            data: [],
                            borderColor: '#f39c12',
                            fill: false
                        }]
                    },
                    options: {
                        responsive: true,
                        scales: {
                            y: {
                                beginAtZero: true
                            }
                        }
                    }
                });
            }
            
            // Initialize motor chart
            const motorCtx = document.getElementById('motorChart');
            if (motorCtx) {
                charts.motorChart = new Chart(motorCtx, {
                    type: 'line',
                    data: {
                        labels: [],
                        datasets: [{
                            label: 'eRPM',
                            data: [],
                            borderColor: '#2ecc71',
                            fill: false
                        }, {
                            label: 'Duty Cycle %',
                            data: [],
                            borderColor: '#9b59b6',
                            fill: false
                        }]
                    },
                    options: {
                        responsive: true,
                        scales: {
                            y: {
                                beginAtZero: true
                            }
                        }
                    }
                });
            }
        }
        
        // Update charts with new data
        function updateCharts(data) {
            if (typeof Chart === 'undefined' || data.length === 0) {
                return;
            }
            
            const labels = data.map((point, index) => index);
            
            // Update temperature chart
            if (charts.tempChart) {
                charts.tempChart.data.labels = labels;
                charts.tempChart.data.datasets[0].data = data.map(point => point.temperature);
                charts.tempChart.update();
            }
            
            // Update battery chart
            if (charts.batteryChart) {
                charts.batteryChart.data.labels = labels;
                charts.batteryChart.data.datasets[0].data = data.map(point => point.batteryVoltage);
                charts.batteryChart.data.datasets[1].data = data.map(point => point.current);
                charts.batteryChart.update();
            }
            
            // Update motor chart
            if (charts.motorChart) {
                charts.motorChart.data.labels = labels;
                charts.motorChart.data.datasets[0].data = data.map(point => point.rpm);
                charts.motorChart.data.datasets[1].data = data.map(point => point.dutyCycle);
                charts.motorChart.update();
            }
        }
        
        // Tab navigation function
        function showTab(tabName) {
            // Hide all tabs
            const tabs = document.querySelectorAll('.tab-content');
            tabs.forEach(tab => {
                tab.style.display = 'none';
            });
            
            // Remove active class from all nav tabs
            const navTabs = document.querySelectorAll('.nav-tab');
            navTabs.forEach(tab => {
                tab.classList.remove('active');
            });
            
            // Show selected tab
            const selectedTab = document.getElementById(tabName + '-tab');
            if (selectedTab) {
                selectedTab.style.display = 'block';
            }
            
            // Add active class to clicked nav tab
            event.target.classList.add('active');
        }

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

        // Check if enough space is available for the file
        size_t htmlSize = strlen(indexHTML);
        if (freeBytes < htmlSize + 1024) { // 1KB buffer for safe estimation
            log("Not enough space in filesystem for index.html");
            String neededMsg = "Required: " + String(htmlSize + 1024);
            log(neededMsg.c_str());
            String availableMsg = "Available: " + String(freeBytes);
            log(availableMsg.c_str());
            return false;
        }

        String startMsg = "Starting to write index.html (Size: " + String(htmlSize) + " bytes)";
        log(startMsg.c_str());
        
        // Try to delete possible old file first
        if (LittleFS.exists("/index.html")) {
            LittleFS.remove("/index.html");
            delay(100);
        }
        
        // Write file in smaller blocks
        File file = LittleFS.open("/index.html", "w");
        if (!file) {
            log("Failed to open file for writing");
            return false;
        }
        
        // Write file in 512-byte blocks (smaller blocks)
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
            
            // Longer pause after each block
            delay(20);
        }
        
        file.flush();
        delay(20);
        file.close();
        delay(20);
        
        // Check if file was written successfully
        if (LittleFS.exists("/index.html")) {
            File checkFile = LittleFS.open("/index.html", "r");
            if (checkFile && checkFile.size() > 0) {
                String successMsg = "index.html successfully saved (" + String(checkFile.size()) + " bytes)";
                log(successMsg.c_str());
                checkFile.close();
            } else {
                log("index.html exists but might be empty");
                if (checkFile) checkFile.close();
                return false;
            }
        } else {
            log("index.html could not be saved");
            return false;
        }
    } else {
        log("index.html already exists, skipping");
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
    
    // Store info.html in LittleFS if it doesn't exist
    if (!LittleFS.exists("/info.html")) {
        log("Creating info.html file...");
        
        const char* infoHTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>DPVControl - System Info</title>
    <style>
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            margin: 0;
            padding: 20px;
            background-color: #e0e5e9;
            color: #1e272e;
        }
        .container {
            max-width: 800px;
            margin: 0 auto;
            background-color: white;
            border-radius: 5px;
            box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
            padding: 30px;
        }
        .logo {
            max-width: 300px;
            margin: 0 auto 30px auto;
            display: block;
        }
        h1 {
            color: #3498db;
            text-align: center;
            margin-bottom: 30px;
        }
        h2 {
            color: #2c3e50;
            border-bottom: 2px solid #3498db;
            padding-bottom: 10px;
        }
        .info-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 20px;
            margin-bottom: 30px;
        }
        .info-card {
            background-color: #f8f9fa;
            padding: 20px;
            border-radius: 8px;
            border-left: 4px solid #3498db;
        }
        .info-label {
            font-weight: bold;
            color: #2c3e50;
            margin-bottom: 5px;
        }
        .info-value {
            font-size: 1.1em;
            color: #34495e;
        }
        .status-indicator {
            display: inline-block;
            width: 12px;
            height: 12px;
            border-radius: 50%;
            margin-right: 8px;
        }
        .status-online {
            background-color: #27ae60;
        }
        .status-offline {
            background-color: #e74c3c;
        }
        .refresh-btn {
            background-color: #3498db;
            color: white;
            border: none;
            padding: 10px 20px;
            border-radius: 4px;
            cursor: pointer;
            margin: 10px 0;
        }
        .refresh-btn:hover {
            background-color: #2980b9;
        }
        .project-link {
            text-align: center;
            margin-top: 30px;
            padding-top: 20px;
            border-top: 1px solid #eee;
        }
        .project-link a {
            color: #3498db;
            text-decoration: none;
        }
        .project-link a:hover {
            text-decoration: underline;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>DPVControl System Information</h1>
        
        <div class="info-grid">
            <div class="info-card">
                <div class="info-label">System Status</div>
                <div class="info-value">
                    <span class="status-indicator" id="statusIndicator"></span>
                    <span id="systemStatus">Checking...</span>
                </div>
            </div>
            
            <div class="info-card">
                <div class="info-label">Firmware Version</div>
                <div class="info-value" id="firmwareVersion">Loading...</div>
            </div>
            
            <div class="info-card">
                <div class="info-label">Uptime (Current Session)</div>
                <div class="info-value" id="currentUptime">Loading...</div>
            </div>
            
            <div class="info-card">
                <div class="info-label">Total Runtime</div>
                <div class="info-value" id="totalUptime">Loading...</div>
            </div>
            
            <div class="info-card">
                <div class="info-label">Data Points in Buffer</div>
                <div class="info-value" id="dataPoints">Loading...</div>
            </div>
            
            <div class="info-card">
                <div class="info-label">Datalogger Status</div>
                <div class="info-value" id="dataloggerStatus">Loading...</div>
            </div>
        </div>
        
        <h2>Sensor Status</h2>
        <div class="info-grid">
            <div class="info-card">
                <div class="info-label">Front Water Sensor</div>
                <div class="info-value" id="waterSensorFront">Loading...</div>
            </div>
            
            <div class="info-card">
                <div class="info-label">Back Water Sensor</div>
                <div class="info-value" id="waterSensorBack">Loading...</div>
            </div>
            
            <div class="info-card">
                <div class="info-label">Left Button</div>
                <div class="info-value" id="leftButton">Loading...</div>
            </div>
            
            <div class="info-card">
                <div class="info-label">Right Button</div>
                <div class="info-value" id="rightButton">Loading...</div>
            </div>
            
            <div class="info-card">
                <div class="info-label">Front Lamp Level</div>
                <div class="info-value" id="lampLevel">Loading...</div>
            </div>
            
            <div class="info-card">
                <div class="info-label">Beeper Status</div>
                <div class="info-value" id="beeperEnabled">Loading...</div>
            </div>
        </div>
        
        <button class="refresh-btn" onclick="loadSystemInfo()">Refresh Information</button>
        
        <div class="project-link">
            <p>For more information about this project, visit:<br>
            <a href="https://bubtec.de/2024/10/29/diy-scooter-dpv-mit-aquazepp-und-dpvcontrol/" target="_blank">
                DPVControl Project Blog
            </a></p>
        </div>
    </div>

    <script>
        // Load system information
        async function loadSystemInfo() {
            try {
                // Load version information
                const versionResponse = await fetch('/api/version');
                if (versionResponse.ok) {
                    const versionData = await versionResponse.json();
                    document.getElementById('firmwareVersion').textContent = versionData.version;
                }
                
                // Load status information
                const statusResponse = await fetch('/api/status');
                if (statusResponse.ok) {
                    const statusData = await statusResponse.json();
                    
                    // Update system status
                    document.getElementById('systemStatus').textContent = 'Online';
                    document.getElementById('statusIndicator').className = 'status-indicator status-online';
                    
                    // Update uptime information
                    document.getElementById('currentUptime').textContent = formatTime(statusData.uptime);
                    document.getElementById('totalUptime').textContent = formatTime(statusData.totalUptime * 1000);
                    
                    // Update data information
                    document.getElementById('dataPoints').textContent = statusData.dataPoints;
                    document.getElementById('dataloggerStatus').textContent = statusData.isDataloggerRunning ? 'Running' : 'Stopped';
                    
                    // Update sensor status
                    document.getElementById('waterSensorFront').textContent = statusData.waterSensorFront ? 'LEAK DETECTED' : 'OK';
                    document.getElementById('waterSensorBack').textContent = statusData.waterSensorBack ? 'LEAK DETECTED' : 'OK';
                    document.getElementById('leftButton').textContent = statusData.leftButton ? 'Pressed' : 'Released';
                    document.getElementById('rightButton').textContent = statusData.rightButton ? 'Pressed' : 'Released';
                    document.getElementById('lampLevel').textContent = statusData.lampLevel;
                    document.getElementById('beeperEnabled').textContent = statusData.beeperEnabled ? 'Enabled' : 'Disabled';
                    
                } else {
                    throw new Error('Failed to load status');
                }
                
            } catch (error) {
                console.error('Error loading system info:', error);
                document.getElementById('systemStatus').textContent = 'Offline';
                document.getElementById('statusIndicator').className = 'status-indicator status-offline';
            }
        }
        
        // Format time in HH:MM:SS
        function formatTime(milliseconds) {
            const totalSeconds = Math.floor(milliseconds / 1000);
            const hours = Math.floor(totalSeconds / 3600);
            const minutes = Math.floor((totalSeconds % 3600) / 60);
            const seconds = totalSeconds % 60;
            
            return hours + 'h ' + minutes + 'm ' + seconds + 's';
        }
        
        // Load information when page loads
        document.addEventListener('DOMContentLoaded', function() {
            loadSystemInfo();
            
            // Auto-refresh every 30 seconds
            setInterval(loadSystemInfo, 30000);
        });
    </script>
</body>
</html>
)rawliteral";
        
        if (!storeFile("/info.html", infoHTML)) {
            log("Failed to store info.html, but continuing anyway");
        } else {
            log("info.html stored successfully");
        }
    } else {
        log("info.html already exists, skipping");
    }
    
    return true;
}

// Store a file in LittleFS
bool storeFile(const char* path, const char* content) {
    String logMsg = "Storing file: " + String(path);
    log(logMsg.c_str());
    
    // Longer delay for hardware operations
    delay(100);
    
    // Try to delete possible old file first
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
    
    // Write file in blocks
    const size_t chunkSize = 512; // Reduced block size
    size_t contentSize = strlen(content);
    size_t remaining = contentSize;
    size_t position = 0;
    
    String sizeMsg = "Writing " + String(contentSize) + " bytes in blocks of " + String(chunkSize) + " bytes";
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
        
        // Longer pause after each block
        delay(20);
    }
    
    file.flush();
    delay(20);
    file.close();
    delay(20);
    
    // Check if file was written successfully
    if (LittleFS.exists(path)) {
        File checkFile = LittleFS.open(path, "r");
        if (checkFile && checkFile.size() > 0) {
            String successMsg = "File successfully saved: " + String(path) + " (" + String(checkFile.size()) + " bytes)";
            log(successMsg.c_str());
            checkFile.close();
            return true;
        } else {
            String emptyMsg = "File exists but might be empty: " + String(path);
            log(emptyMsg.c_str());
            if (checkFile) checkFile.close();
            return false;
        }
    } else {
        String failMsg = "File could not be saved: " + String(path);
        log(failMsg.c_str());
        return false;
    }
} 