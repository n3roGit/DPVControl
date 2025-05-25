#include "webserver.h"
#include "log.h"
#include "data_upload.h"
#include "datalog.h"  // Einbinden des Datalogger-Headers
#include "constants.h" // Für PIN-Definitionen

// External variables
extern int LED_State; // From ledLamp.cpp

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
                        <td>RPM:</td>
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
                            <label for="timeRange">Time Range:</label>
                            <select id="timeRange" onchange="updateTimeRange()">
                                <option value="live">Live (last 60s)</option>
                                <option value="recent">Recent (8 minutes)</option>
                                <option value="hourly">Hourly (48 minutes)</option>
                                <option value="historical">Historical (2 hours)</option>
                                <option value="all">All available data</option>
                            </select>
                        </div>
                        
                        <div id="timeSliderContainer" style="display: none; flex: 1; min-width: 200px;">
                            <label for="timeSlider">Time Window Position:</label>
                            <input type="range" id="timeSlider" min="0" max="100" value="100" 
                                   style="width: 100%;" onchange="updateTimeWindow()">
                            <div style="display: flex; justify-content: space-between; font-size: 12px; color: #666;">
                                <span id="sliderStart">Oldest</span>
                                <span id="sliderEnd">Newest</span>
                            </div>
                        </div>
                        
                        <div>
                            <label for="updateInterval">Update (s):</label>
                            <input type="number" id="updateInterval" min="1" max="60" value="5" 
                                   style="width: 60px;" onchange="saveSettings()">
                        </div>
                        
                        <button class="button" onclick="refreshChart()">Refresh</button>
                    </div>
                </div>
                
                <!-- Chart Container -->
                <div style="height: 400px; width: 100%; position: relative;">
                    <canvas id="combinedChart"></canvas>
                </div>
                
                <p style="margin-top: 10px; font-size: 12px; color: #666;">
                    Note: Different parameters use different scales. Red vertical lines indicate system restarts.
                </p>
            </div>
        </div>
        
        <div id="settings-tab" class="tab-content">
            <div class="section">
                <h2>Settings</h2>
                <p>Chart settings have been moved to the Charts tab for better usability.</p>
                <p>You can still configure data display settings in the Data tab.</p>
            </div>
        </div>
    </div>

    <script>
        // Variables
        let updateInterval = 5000; // 5 seconds
        let dataPointsToShow = 20;
        let charts = {};
        let allDataPoints = [];
        let currentTimeRange = 'live';
        let timeSliderValue = 100;
        let systemStartTime = null;
        
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
            
            if (localStorage.getItem('timeRange')) {
                currentTimeRange = localStorage.getItem('timeRange');
                document.getElementById('timeRange').value = currentTimeRange;
            }
            
            // Initialize charts
            initCharts();
            
            // First data load
            loadData();
            
            // Set up periodic updates
            setInterval(loadData, updateInterval);
            
            // Update time range controls
            updateTimeRange();
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
                        },
                        annotation: {
                            annotations: {}
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
        
        // Save settings
        function saveSettings() {
            updateInterval = parseInt(document.getElementById('updateInterval').value) * 1000;
            localStorage.setItem('updateInterval', updateInterval / 1000);
            localStorage.setItem('timeRange', currentTimeRange);
        }
        
        // Update time range
        function updateTimeRange() {
            currentTimeRange = document.getElementById('timeRange').value;
            const sliderContainer = document.getElementById('timeSliderContainer');
            
            if (currentTimeRange === 'live' || currentTimeRange === 'all') {
                sliderContainer.style.display = 'none';
            } else {
                sliderContainer.style.display = 'block';
            }
            
            saveSettings();
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
        
        // Calculate data points needed based on time range
        function getDataPointsForTimeRange() {
            switch(currentTimeRange) {
                case 'live': return 60; // 60 seconds
                case 'recent': return 500; // 8.3 minutes (recent buffer)
                case 'hourly': return 48; // 48 minutes (hourly buffer)
                case 'historical': return 24; // 2 hours (historical buffer)
                case 'all': return 500; // All available (max recent buffer size)
                default: return 60;
            }
        }
        
        // Filter data based on time range and slider position
        function filterDataByTimeRange(data) {
            if (currentTimeRange === 'live' || currentTimeRange === 'all') {
                return data;
            }
            
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
        
        // Detect system restart (gap in timestamps > 2 minutes)
        function detectRestarts(data) {
            const restarts = [];
            for (let i = 1; i < data.length; i++) {
                const timeDiff = data[i].timestamp - data[i-1].timestamp;
                if (timeDiff > 120000) { // 2 minutes gap
                    restarts.push(i);
                }
            }
            return restarts;
        }
        
        // Load data from the API
        function loadData() {
            // Fetch status data
            fetch('/api/status')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('uptime').textContent = formatTime(data.uptime);
                    document.getElementById('totalUptime').textContent = formatTime(data.totalUptime * 1000);
                    document.getElementById('dataPointCount').textContent = data.dataPoints || 0;
                    
                    // Update additional sensor data
                    document.getElementById('waterSensorFront').textContent = data.waterSensorFront === 'true' ? 'LEAK DETECTED!' : 'OK';
                    document.getElementById('waterSensorBack').textContent = data.waterSensorBack === 'true' ? 'LEAK DETECTED!' : 'OK';
                    document.getElementById('leftButton').textContent = data.leftButton === 'true' ? 'PRESSED' : 'RELEASED';
                    document.getElementById('rightButton').textContent = data.rightButton === 'true' ? 'PRESSED' : 'RELEASED';
                    document.getElementById('lampLevel').textContent = 'Level ' + data.lampLevel + ' (0-4)';
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
        }
        
        // Load chart data based on current settings
        function loadChartData() {
            const pointsNeeded = getDataPointsForTimeRange();
            let apiUrl = '/api/data?count=' + pointsNeeded;
            
            // Add range parameter for non-live data
            if (currentTimeRange === 'hourly') {
                apiUrl += '&range=hourly';
            } else if (currentTimeRange === 'historical') {
                apiUrl += '&range=historical';
            } else if (currentTimeRange === 'recent' || currentTimeRange === 'all') {
                apiUrl += '&range=recent';
            }
            // live uses recent data by default
            
            fetch(apiUrl)
                .then(response => response.json())
                .then(data => {
                    allDataPoints = data;
                    updateCharts(data);
                })
                .catch(error => {
                    console.error('Error fetching chart data:', error);
                });
        }
        
        // Update charts with new data
        function updateCharts(data) {
            if (data.length === 0) return;
            
            // Filter data based on time range and slider
            const filteredData = filterDataByTimeRange(data);
            
            // Detect restarts
            const restarts = detectRestarts(filteredData);
            
            // Prepare labels (timestamps)
            const labels = filteredData.map(item => {
                const date = new Date(item.timestamp);
                return date.toLocaleTimeString();
            });
            
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
            
            // Add restart markers
            charts.combinedChart.options.plugins.annotation.annotations = {};
            restarts.forEach((restartIndex, i) => {
                charts.combinedChart.options.plugins.annotation.annotations['restart' + i] = {
                    type: 'line',
                    xMin: restartIndex,
                    xMax: restartIndex,
                    borderColor: 'red',
                    borderWidth: 2,
                    borderDash: [5, 5],
                    label: {
                        content: 'System Restart',
                        enabled: true,
                        position: 'top'
                    }
                };
            });
            
            charts.combinedChart.update('none');
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
        jsonStatus += "\"lampLevel\":" + String(LED_State);
        jsonStatus += "}";
        
        String statusMsg = "Status response: " + jsonStatus;
        log(statusMsg.c_str());
        
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