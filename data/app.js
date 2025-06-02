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
            
            // Load additional tab content
            loadTabContent();
        } else {
            console.log('Waiting for libraries to load... Chart:', typeof Chart, 'JSZip:', typeof JSZip);
            setTimeout(waitForLibraries, 200);
        }
    }
    
    // Start waiting for libraries
    setTimeout(waitForLibraries, 500); // Give initial load time
});

// Load additional tab content via AJAX
function loadTabContent() {
    // Load remote control tab
    fetch('/remote.html')
        .then(response => response.text())
        .then(html => {
            document.getElementById('remote-tab').innerHTML = html;
        })
        .catch(error => {
            console.error('Error loading remote control tab:', error);
            document.getElementById('remote-tab').innerHTML = '<div class="section"><h2>Remote Control</h2><p>Error loading remote control interface.</p></div>';
        });
    
    // Load settings tab
    fetch('/settings.html')
        .then(response => response.text())
        .then(html => {
            document.getElementById('settings-tab').innerHTML = html;
            // Initialize settings after loading
            setTimeout(() => {
                updateLampBrightnessInputs();
                loadDPVSettings();
            }, 100);
        })
        .catch(error => {
            console.error('Error loading settings tab:', error);
            document.getElementById('settings-tab').innerHTML = '<div class="section"><h2>Settings</h2><p>Error loading settings interface.</p></div>';
        });
    
    // Load info tab
    fetch('/info.html')
        .then(response => response.text())
        .then(html => {
            document.getElementById('info-tab').innerHTML = html;
            // Load version info after loading
            setTimeout(loadVersionInfo, 100);
        })
        .catch(error => {
            console.error('Error loading info tab:', error);
            document.getElementById('info-tab').innerHTML = '<div class="section"><h2>Info</h2><p>Error loading info interface.</p></div>';
        });
}

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

// Built-in Chart class optimized for DPV data
if (typeof Chart === 'undefined') {
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
}

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
}

// Format time in HH:MM:SS
function formatTime(milliseconds) {
    const totalSeconds = Math.floor(milliseconds / 1000);
    const hours = Math.floor(totalSeconds / 3600);
    const minutes = Math.floor((totalSeconds % 3600) / 60);
    const seconds = totalSeconds % 60;
    
    return String(hours).padStart(2, '0') + ':' + String(minutes).padStart(2, '0') + ':' + String(seconds).padStart(2, '0');
}

// Additional functions would be included here...
// This is a condensed version. The complete JavaScript file would include all functions from the original webserver.cpp

// Placeholder functions for now - these would be implemented fully
function loadSessionList() { console.log('loadSessionList called'); }
function loadChartData() { console.log('loadChartData called'); }
function loadVersionInfo() { console.log('loadVersionInfo called'); }
function updateLampBrightnessInputs() { console.log('updateLampBrightnessInputs called'); }
function loadDPVSettings() { console.log('loadDPVSettings called'); }
function enableRemoteControlInterface() { console.log('enableRemoteControlInterface called'); }
function loadDataWithLiveSession() { loadData(); }
function updateSessionFilter() { console.log('updateSessionFilter called'); }
function updateTimeWindow() { console.log('updateTimeWindow called'); }
function refreshChart() { console.log('refreshChart called'); }
function deleteAllSessions() { console.log('deleteAllSessions called'); }
function exportCurrentViewAsCSV() { console.log('exportCurrentViewAsCSV called'); }
function exportAllSessionsAsZip() { console.log('exportAllSessionsAsZip called'); } 