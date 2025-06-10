// Variables
let updateInterval = 10000; // Fixed 10 seconds
let charts = {};
let allDataPoints = [];
let sessionMetadata = null; // Store session metadata (real duration, total datapoints, etc.)
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
            
            // Generate lamp levels after loading remote page
            setTimeout(generateLampLevels, 100);
            
            // Initialize remote control interface
            setTimeout(enableRemoteControlInterface, 200);
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
    
    // Create proper CSV header matching backend format
    let csv = "Total Uptime (s),Motor Temperature (degC),MOSFET Temperature (degC),Battery Voltage (V),Input Current (A),Motor Current (A),eRPM,Duty Cycle (%),Ambient Temperature (degC),Humidity (%),Battery Level (%),Leak Sensor State,LED Brightness (%),Left Button,Right Button,Beeper Enabled,Beeper Active\r\n";
    
    data.forEach(item => {
        // Convert totalUptime from milliseconds to seconds
        const totalUptimeSeconds = (item.totalUptime || 0) / 1000.0;
        
        // Ensure boolean values are properly converted to 0 or 1
        const beeperActive = item.beeperActive ? 1 : 0;
        const leftButton = item.leftButton ? 1 : 0;
        const rightButton = item.rightButton ? 1 : 0;
        const leakSensorState = item.leakSensorState ? 1 : 0;
        const ledBrightness = item.ledBrightness || 0;
        
        csv += [
            totalUptimeSeconds.toFixed(1),
            (item.tempMotor || 0).toFixed(1),
            (item.tempMosfet || 0).toFixed(1),
            (item.batteryVoltage || 0).toFixed(5),
            (item.current || 0).toFixed(2),
            (item.avgMotorCurrent || 0).toFixed(2),
            item.erpm || 0,
            (item.dutyCycle || 0).toFixed(3),
            (item.temperature || 0).toFixed(1),
            (item.humidity || 0).toFixed(1),
            item.batteryLevel || 0,
            leakSensorState,
            ledBrightness,
            leftButton,
            rightButton,
            item.beeperEnabled ? 1 : 0,
            beeperActive
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

// Initialize Charts
function initCharts() {
    // Combined Chart with all data
    const combinedCtx = document.getElementById('combinedChart').getContext('2d');
    const chartConfig = {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'Battery Voltage (V)',
                borderColor: 'rgb(0, 255, 255)', // Cyan for electrical
                backgroundColor: 'rgba(0, 255, 255, 0.1)',
                borderWidth: 2,
                data: [],
                tension: 0.2,
                yAxisID: 'voltage'
            }, {
                label: 'Current (A)',
                borderColor: 'rgb(255, 100, 100)', // Red for danger/high current
                backgroundColor: 'rgba(255, 100, 100, 0.1)',
                borderWidth: 2,
                data: [],
                tension: 0.2,
                yAxisID: 'current'
            }, {
                label: 'Motor Temp (°C)',
                borderColor: 'rgb(255, 165, 0)', // Orange for heat
                backgroundColor: 'rgba(255, 165, 0, 0.1)',
                borderWidth: 2,
                data: [],
                tension: 0.2,
                yAxisID: 'temperature'
            }, {
                label: 'Ambient Temp (°C)',
                borderColor: 'rgb(255, 215, 0)', // Gold for ambient heat
                backgroundColor: 'rgba(255, 215, 0, 0.1)',
                borderWidth: 2,
                data: [],
                tension: 0.2,
                yAxisID: 'temperature'
            }, {
                label: 'Humidity (%)',
                borderColor: 'rgb(30, 144, 255)', // Blue for water/humidity
                backgroundColor: 'rgba(30, 144, 255, 0.1)',
                borderWidth: 2,
                data: [],
                tension: 0.2,
                yAxisID: 'percent'
            }, {
                label: 'eRPM',
                borderColor: 'rgb(255, 20, 147)', // Deep pink for motor
                backgroundColor: 'rgba(255, 20, 147, 0.1)',
                borderWidth: 2,
                data: [],
                tension: 0.2,
                yAxisID: 'rpm'
            }, {
                label: 'Duty Cycle (%)',
                borderColor: 'rgb(150, 150, 150)', // Gray for duty cycle
                backgroundColor: 'rgba(150, 150, 150, 0.1)',
                borderWidth: 2,
                data: [],
                tension: 0.2,
                yAxisID: 'percent'
            }, {
                label: 'MOSFET Temp (°C)',
                borderColor: 'rgb(255, 140, 0)', // Dark orange for MOSFET heat
                backgroundColor: 'rgba(255, 140, 0, 0.1)',
                borderWidth: 2,
                data: [],
                tension: 0.2,
                yAxisID: 'temperature'
            }, {
                label: 'Motor Current (A)',
                borderColor: 'rgb(220, 20, 60)', // Crimson for motor current
                backgroundColor: 'rgba(220, 20, 60, 0.1)',
                borderWidth: 2,
                data: [],
                tension: 0.2,
                yAxisID: 'current'
            }, {
                label: 'Battery Level (%)',
                borderColor: 'rgb(50, 205, 50)', // Lime green for battery energy
                backgroundColor: 'rgba(50, 205, 50, 0.1)',
                borderWidth: 2,
                data: [],
                tension: 0.2,
                yAxisID: 'percent'
            }, {
                label: 'Beeper Active',
                borderColor: 'rgb(255, 140, 0)', // Orange for warning beeper
                backgroundColor: 'rgba(255, 140, 0, 0.1)',
                borderWidth: 3,
                data: [],
                tension: 0,
                stepped: true,
                yAxisID: 'boolean'
            }, {
                label: 'Left Button',
                borderColor: 'rgb(34, 139, 34)', // Forest green for left
                backgroundColor: 'rgba(34, 139, 34, 0.1)',
                borderWidth: 3,
                data: [],
                tension: 0,
                stepped: true,
                yAxisID: 'boolean'
            }, {
                label: 'Right Button',
                borderColor: 'rgb(178, 34, 34)', // Fire brick red for right
                backgroundColor: 'rgba(178, 34, 34, 0.1)',
                borderWidth: 3,
                data: [],
                tension: 0,
                stepped: true,
                yAxisID: 'boolean'
            }, {
                label: 'Leak Sensor',
                borderColor: 'rgb(255, 69, 0)', // Red orange for danger/leak
                backgroundColor: 'rgba(255, 69, 0, 0.2)',
                borderWidth: 4,
                data: [],
                tension: 0,
                stepped: true,
                yAxisID: 'boolean'
            }, {
                label: 'LED Brightness (%)',
                borderColor: 'rgb(255, 255, 0)', // Yellow for light/LED
                backgroundColor: 'rgba(255, 255, 0, 0.2)',
                borderWidth: 3,
                data: [],
                tension: 0,
                stepped: true,
                yAxisID: 'percent'
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: {
                duration: 500
            },
            interaction: {
                mode: 'index',
                intersect: false
            },
            scales: {
                x: {
                    title: {
                        display: true,
                        text: 'Runtime (mm:ss)',
                        color: '#aaa'
                    },
                    grid: {
                        color: 'rgba(255, 255, 255, 0.1)'
                    },
                    ticks: {
                        color: '#aaa',
                        callback: function(value) {
                            // Convert seconds to mm:ss format
                            const totalSeconds = Math.floor(value);
                            const minutes = Math.floor(totalSeconds / 60);
                            const seconds = totalSeconds % 60;
                            return String(minutes).padStart(2, '0') + ':' + String(seconds).padStart(2, '0');
                        }
                    }
                },
                voltage: {
                    type: 'linear',
                    display: true,
                    position: 'left',
                    title: {
                        display: true,
                        text: 'Voltage (V)',
                        color: 'rgb(0, 255, 255)'
                    },
                    grid: {
                        color: 'rgba(0, 255, 255, 0.2)'
                    },
                    ticks: {
                        color: '#aaa'
                    }
                },
                current: {
                    type: 'linear',
                    display: true,
                    position: 'right',
                    title: {
                        display: true,
                        text: 'Current (A)',
                        color: 'rgb(255, 100, 100)'
                    },
                    grid: {
                        display: false
                    },
                    ticks: {
                        color: '#aaa'
                    }
                },
                temperature: {
                    type: 'linear',
                    display: true,
                    position: 'left',
                    title: {
                        display: true,
                        text: 'Temperature (°C)',
                        color: 'rgb(255, 165, 0)'
                    },
                    grid: {
                        display: false
                    },
                    ticks: {
                        color: '#aaa'
                    }
                },
                percent: {
                    type: 'linear',
                    display: true,
                    position: 'right',
                    title: {
                        display: true,
                        text: 'Percent (%)',
                        color: 'rgb(50, 205, 50)'
                    },
                    min: 0,
                    max: 100,
                    grid: {
                        display: false
                    },
                    ticks: {
                        color: '#aaa'
                    }
                },
                rpm: {
                    type: 'linear',
                    display: false,
                    position: 'right',
                    title: {
                        display: true,
                        text: 'eRPM',
                        color: 'rgb(255, 20, 147)'
                    },
                    grid: {
                        display: false
                    },
                    ticks: {
                        color: '#aaa'
                    }
                },
                boolean: {
                    type: 'linear',
                    display: true,
                    position: 'right',
                    title: {
                        display: true,
                        text: 'Boolean (0/1)',
                        color: 'rgb(255, 255, 0)'
                    },
                    min: -0.1,
                    max: 1.1,
                    grid: {
                        display: false
                    },
                    ticks: {
                        color: '#aaa',
                        stepSize: 1,
                        callback: function(value) {
                            return value === 0 ? 'OFF' : value === 1 ? 'ON' : '';
                        }
                    }
                }
            },
            plugins: {
                legend: {
                    position: 'top',
                    labels: {
                        usePointStyle: true,
                        boxWidth: 10,
                        color: '#ddd'
                    }
                },
                tooltip: {
                    backgroundColor: 'rgba(0, 0, 0, 0.8)',
                    titleColor: '#fff',
                    bodyColor: '#fff',
                    titleFont: {
                        size: 14
                    },
                    bodyFont: {
                        size: 13
                    },
                    padding: 10,
                    caretSize: 8,
                    cornerRadius: 4,
                    displayColors: true
                },
                zoom: {
                    pan: {
                        enabled: true,
                        mode: 'x',
                        threshold: 5
                    },
                    zoom: {
                        wheel: {
                            enabled: true,
                        },
                        pinch: {
                            enabled: true
                        },
                        mode: 'x'
                    }
                }
            }
        }
    };

    charts.combinedChart = new Chart(combinedCtx, chartConfig);
    console.log('Chart.js visualization initialized with zoom plugin');
}

// Tab Navigation
function showTab(tabName, event) {
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
    if (event && event.target) {
        event.target.classList.add('active');
    }
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
            
            const rpm = document.getElementById('rpm');
            if (rpm) rpm.textContent = data.erpm ? data.erpm + ' RPM' : '0 RPM';
            
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
function loadSessionList() {
    console.log('Loading session list...');
    fetch('/api/sessions')
        .then(response => response.json())
        .then(sessions => {
            console.log('Sessions loaded:', sessions);
            availableSessions = sessions;
            const select = document.getElementById('sessionSelect');
            select.innerHTML = '';
            
            sessions.forEach(session => {
                const option = document.createElement('option');
                option.value = session.filename;
                option.text = session.displayName;
                if (session.isCurrent) {
                    option.selected = true;
                    selectedSession = session.filename;
                }
                select.appendChild(option);
            });
            
            // Load initial chart data
            loadChartData();
        })
        .catch(error => {
            console.error('Error loading sessions:', error);
        });
}

function loadChartData() {
    if (!selectedSession) return;
    
    console.log('Loading chart data for session:', selectedSession);
    fetch(`/api/sessions/${selectedSession}/data`)
        .then(response => response.json())
        .then(data => {
            console.log('Chart data loaded:', data);
            sessionMetadata = data.meta;
            allDataPoints = data.data;
            
            // Update chart with all data (native zoom will handle windowing)
            updateChartData();
        })
        .catch(error => {
            console.error('Error loading chart data:', error);
        });
}

function updateChartData() {
    if (!allDataPoints || !charts.combinedChart) return;
    
    console.log('Updating chart with', allDataPoints.length, 'data points');
    
    // Debug: Log first and last totalUptime values
    if (allDataPoints.length > 0) {
        const firstPoint = allDataPoints[0];
        const lastPoint = allDataPoints[allDataPoints.length - 1];
        console.log(`Raw totalUptime range: ${firstPoint.totalUptime}ms to ${lastPoint.totalUptime}ms`);
        console.log(`Raw totalUptime difference: ${(lastPoint.totalUptime - firstPoint.totalUptime) / 1000}s`);
    }
    
    // Calculate session-relative time instead of total uptime
    // Find the first data point's timestamp to use as session start reference
    let sessionStartTime = allDataPoints.length > 0 ? allDataPoints[0].totalUptime : 0;
    
    // For better user experience, if we have session metadata, use that
    if (sessionMetadata && sessionMetadata.realStartTimestamp) {
        console.log(`Using session metadata start time: ${sessionMetadata.realStartTimestamp}ms`);
        sessionStartTime = sessionMetadata.realStartTimestamp;
    } else {
        console.log(`Using first datapoint as start time: ${sessionStartTime}ms`);
    }
    
    // Create session-relative time data (seconds from session start)
    const xData = allDataPoints.map(d => {
        const relativeTime = ((d.totalUptime || 0) - sessionStartTime) / 1000;
        return Math.max(0, relativeTime); // Ensure no negative times
    });
    
    console.log(`Session time range: 0s to ${Math.max(...xData).toFixed(1)}s (${(Math.max(...xData)/60).toFixed(1)} minutes)`);
    console.log(`Session metadata:`, sessionMetadata);
    
    // Update chart data
    charts.combinedChart.data.labels = xData;
    charts.combinedChart.data.datasets[0].data = allDataPoints.map((d, i) => ({x: xData[i], y: d.batteryVoltage}));
    charts.combinedChart.data.datasets[1].data = allDataPoints.map((d, i) => ({x: xData[i], y: d.current}));
    charts.combinedChart.data.datasets[2].data = allDataPoints.map((d, i) => ({x: xData[i], y: d.tempMotor}));
    charts.combinedChart.data.datasets[3].data = allDataPoints.map((d, i) => ({x: xData[i], y: d.temperature}));
    charts.combinedChart.data.datasets[4].data = allDataPoints.map((d, i) => ({x: xData[i], y: d.humidity}));
    charts.combinedChart.data.datasets[5].data = allDataPoints.map((d, i) => ({x: xData[i], y: d.erpm}));
    charts.combinedChart.data.datasets[6].data = allDataPoints.map((d, i) => ({x: xData[i], y: d.dutyCycle}));
    charts.combinedChart.data.datasets[7].data = allDataPoints.map((d, i) => ({x: xData[i], y: d.tempMosfet}));
    charts.combinedChart.data.datasets[8].data = allDataPoints.map((d, i) => ({x: xData[i], y: d.avgMotorCurrent}));
    charts.combinedChart.data.datasets[9].data = allDataPoints.map((d, i) => ({x: xData[i], y: d.batteryLevel}));
    charts.combinedChart.data.datasets[10].data = allDataPoints.map((d, i) => ({x: xData[i], y: d.beeperActive ? 1 : 0}));
    charts.combinedChart.data.datasets[11].data = allDataPoints.map((d, i) => ({x: xData[i], y: d.leftButton ? 1 : 0}));
    charts.combinedChart.data.datasets[12].data = allDataPoints.map((d, i) => ({x: xData[i], y: d.rightButton ? 1 : 0}));
    charts.combinedChart.data.datasets[13].data = allDataPoints.map((d, i) => ({x: xData[i], y: d.leakSensorState ? 1 : 0}));
    charts.combinedChart.data.datasets[14].data = allDataPoints.map((d, i) => ({x: xData[i], y: d.ledBrightness || 0}));
    
    // Reset zoom to show all data by clearing min/max constraints
    if (charts.combinedChart.options.scales.x.min !== undefined || 
        charts.combinedChart.options.scales.x.max !== undefined) {
        delete charts.combinedChart.options.scales.x.min;
        delete charts.combinedChart.options.scales.x.max;
    }
    
    // Update chart
    charts.combinedChart.update('none'); // No animation for better performance
}

function loadVersionInfo() {
    fetch('/api/version')
        .then(response => response.json())
        .then(data => {
            const versionElement = document.getElementById('currentVersion');
            if (versionElement) {
                versionElement.textContent = data.version;
            }
        })
        .catch(error => {
            console.error('Error loading version info:', error);
        });
    fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            const uptime = document.getElementById('systemUptime');
            if (uptime) uptime.textContent = formatTime(data.uptime || 0);
            const totalUptime = document.getElementById('totalRuntime');
            if (totalUptime) totalUptime.textContent = formatTime((data.totalUptime || 0) * 1000);
        })
        .catch(error => {
            console.error('Error loading status info:', error);
        });
}

function updateLampBrightnessInputs() {
    fetch('/api/settings')
        .then(response => response.json())
        .then(settings => {
            const maxLevels = settings.lampMaxLevels;
            const container = document.getElementById('lampBrightnessContainer');
            if (!container) return;
            
            container.innerHTML = '';
            
            for (let i = 1; i < maxLevels; i++) {
                const div = document.createElement('div');
                div.className = 'lamp-level';
                
                const label = document.createElement('label');
                label.textContent = `Level ${i}:`;
                
                const input = document.createElement('input');
                input.type = 'range';
                input.min = '0';
                input.max = '100';
                // Convert PWM value (0-255) to percentage (0-100)
                const pwmValue = settings.lampBrightness[i] || 0;
                const percentageValue = Math.round((pwmValue / 255) * 100);
                input.value = percentageValue;
                input.onchange = () => updateLampBrightness(i, input.value);
                
                const value = document.createElement('span');
                value.textContent = `${input.value}%`;
                input.oninput = () => value.textContent = `${input.value}%`;
                
                div.appendChild(label);
                div.appendChild(input);
                div.appendChild(value);
                container.appendChild(div);
            }
        })
        .catch(error => {
            console.error('Error loading lamp settings:', error);
        });
}

function loadDPVSettings() {
    fetch('/api/settings')
        .then(response => response.json())
        .then(settings => {
            // Update all settings inputs
            Object.keys(settings).forEach(key => {
                const input = document.getElementById(key);
                if (input) {
                    if (typeof settings[key] === 'boolean') {
                        input.checked = settings[key];
                    } else {
                        input.value = settings[key];
                    }
                }
            });
        })
        .catch(error => {
            console.error('Error loading settings:', error);
        });
}

function enableRemoteControlInterface() {
    const speedSlider = document.getElementById('speedSlider');
    const lampSlider = document.getElementById('lampSlider');
    
    if (speedSlider) {
        speedSlider.oninput = function() {
            const speed = this.value;
            document.getElementById('speedValue').textContent = `${speed}%`;
            
            // Send speed update to API
            fetch('/api/motor', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    enabled: speed > 0,
                    speed: parseInt(speed)
                })
            }).catch(error => {
                console.error('Error updating motor speed:', error);
            });
        };
    }
    
    if (lampSlider) {
        lampSlider.oninput = function() {
            const level = this.value;
            document.getElementById('lampValue').textContent = `Level ${level}`;
            
            // Send lamp update to API
            fetch('/api/lamp', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    level: parseInt(level)
                })
            }).catch(error => {
                console.error('Error updating lamp level:', error);
            });
        };
    }
}

function loadDataWithLiveSession() { 
    loadData(); 
    
    // If we're viewing the current live session, refresh with truly live data from RAM buffer
    if (selectedSession && selectedSession.includes('current')) {
        console.log('Refreshing live session with current RAM data');
        
        // Use /api/data endpoint to get current RAM buffer data instead of session file
        fetch('/api/data?count=250') // Get up to 250 recent points from RAM buffer
            .then(response => response.json())
            .then(data => {
                console.log('Live RAM data loaded:', data.length, 'points');
                
                // Create minimal metadata for live data
                sessionMetadata = {
                    isLiveData: true,
                    totalDatapoints: data.length,
                    chartDatapoints: data.length
                };
                allDataPoints = data;
                
                // Update chart with live data
                updateChartData();
                
                // Auto-zoom to show the latest data after refresh
                setTimeout(() => {
                    if (charts.combinedChart && allDataPoints.length > 0) {
                        // For live data, use the first point as session start
                        const firstDataPoint = allDataPoints[0];
                        const lastDataPoint = allDataPoints[allDataPoints.length - 1];
                        
                        if (lastDataPoint && firstDataPoint) {
                            // Calculate session-relative times from first data point
                            const sessionStartTime = firstDataPoint.totalUptime;
                            const maxSessionTime = ((lastDataPoint.totalUptime || 0) - sessionStartTime) / 1000;
                            const totalSessionDuration = maxSessionTime;
                            
                            console.log(`Live session duration: ${totalSessionDuration.toFixed(1)}s`);
                            
                            // For live sessions, show the last 3 minutes or the entire session if shorter
                            const viewWindowSeconds = Math.min(180, totalSessionDuration);
                            const minTime = Math.max(0, maxSessionTime - viewWindowSeconds);
                            
                            // Only apply auto-zoom if the session is longer than the view window
                            if (totalSessionDuration > viewWindowSeconds) {
                                charts.combinedChart.options.scales.x.min = minTime;
                                charts.combinedChart.options.scales.x.max = maxSessionTime;
                                charts.combinedChart.update('none');
                                
                                console.log(`Auto-zoomed live data to show last ${viewWindowSeconds}s (${minTime.toFixed(1)}s to ${maxSessionTime.toFixed(1)}s)`);
                            } else {
                                console.log(`Live session duration ${totalSessionDuration.toFixed(1)}s fits in view window, showing all data`);
                            }
                        }
                    }
                }, 200);
            })
            .catch(error => {
                console.error('Error loading live RAM data, falling back to session file:', error);
                // Fallback to session file method
                loadChartData();
            });
    }
}

function updateSessionFilter() {
    const select = document.getElementById('sessionSelect');
    selectedSession = select.value;
    loadChartData();
}
function refreshChart() {
    loadChartData();
}
function deleteAllSessions() {
    if (!confirm('Are you sure you want to delete all sessions? This cannot be undone.')) {
        return;
    }
    
    fetch('/api/delete-all-sessions', {
        method: 'POST'
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            alert(`Successfully deleted ${data.deleted} sessions`);
            loadSessionList();
        } else {
            alert('Error deleting sessions: ' + data.error);
        }
    })
    .catch(error => {
        console.error('Error deleting sessions:', error);
        alert('Error deleting sessions');
    });
}
function exportCurrentViewAsCSV() {
    if (!selectedSession) {
        alert('No session selected');
        return;
    }
    
    // Use backend CSV export to get complete session data
    const filename = selectedSession.replace('.bin', '.csv');
    
    // Fetch complete session data as CSV from backend
    fetch(`/api/sessions/${selectedSession}/csv`)
        .then(response => {
            if (!response.ok) {
                throw new Error('Failed to export session data');
            }
            return response.text();
        })
        .then(csv => {
            // Create download
            const blob = new Blob([csv], { type: 'text/csv;charset=utf-8' });
            const url = window.URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = filename;
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            window.URL.revokeObjectURL(url);
            
            console.log('Complete session CSV exported: ' + filename);
        })
        .catch(error => {
            console.error('Error exporting session CSV:', error);
            alert('Error exporting session data. Falling back to current view data.');
            
            // Fallback: export currently visible data, filter out corrupted data points
            const cleanData = allDataPoints.filter(item => {
                // Filter out test data and corrupted boolean values
                return item.batteryVoltage > 20 && // Real battery voltage should be > 20V
                       (item.leftButton === 0 || item.leftButton === 1) && // Clean boolean values
                       (item.rightButton === 0 || item.rightButton === 1) &&
                       (item.beeperEnabled === 0 || item.beeperEnabled === 1) &&
                       (item.beeperActive === 0 || item.beeperActive === 1);
            });
            
            if (cleanData.length > 0) {
                exportToCSV(cleanData, filename);
            } else {
                alert('No valid data points available for export.');
            }
        });
}
function exportAllSessionsAsZip() {
    if (!availableSessions || availableSessions.length === 0) {
        alert('No sessions available');
        return;
    }
    
    const zip = new JSZip();
    let completedExports = 0;
    
    availableSessions.forEach(session => {
        fetch(`/api/sessions/${session.filename}/csv`)
            .then(response => response.text())
            .then(csv => {
                const filename = session.filename.replace('.bin', '.csv');
                zip.file(filename, csv);
                completedExports++;
                
                if (completedExports === availableSessions.length) {
                    zip.generateAsync({type: 'blob'})
                        .then(content => {
                            const url = window.URL.createObjectURL(content);
                            const a = document.createElement('a');
                            a.href = url;
                            a.download = 'dpv_sessions.zip';
                            document.body.appendChild(a);
                            a.click();
                            document.body.removeChild(a);
                            window.URL.revokeObjectURL(url);
                        });
                }
            })
            .catch(error => {
                console.error('Error exporting session:', error);
                completedExports++;
            });
    });
}
// Time slider functionality removed - using native Chart.js zoom instead

// Remote Control Functions
function updateMotorSpeed(value) {
    // Show current value in UI
    document.getElementById('motorSpeedValue').textContent = value;
    if (document.getElementById('remoteMotorSpeed')) {
        document.getElementById('remoteMotorSpeed').textContent = value + '%';
    }
}

function setMotorSpeed(value) {
    // Send value to API
    fetch('/api/motor', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
            enabled: value > 0,
            speed: parseInt(value)
        })
    }).then(() => {
        if (document.getElementById('remoteLastCommand')) {
            document.getElementById('remoteLastCommand').textContent = 'Set speed to ' + value + '%';
        }
        
        // Update motor status
        if (document.getElementById('remoteMotorStatus')) {
            document.getElementById('remoteMotorStatus').textContent = value > 0 ? 'RUNNING' : 'STOPPED';
        }
    }).catch(error => {
        console.error('Error updating motor speed:', error);
    });
}

function toggleMotor() {
    // Check current status
    const button = document.getElementById('motorToggle');
    const isRunning = button.textContent.includes('STOP');
    
    if (isRunning) {
        // Stop motor
        fetch('/api/motor', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ enabled: false, speed: 0 })
        }).then(() => {
            button.textContent = 'START MOTOR';
            button.style.backgroundColor = '#4caf50';
            
            // Update UI
            if (document.getElementById('motorSpeedSlider')) {
                document.getElementById('motorSpeedSlider').value = 0;
            }
            if (document.getElementById('motorSpeedValue')) {
                document.getElementById('motorSpeedValue').textContent = '0';
            }
            if (document.getElementById('remoteMotorStatus')) {
                document.getElementById('remoteMotorStatus').textContent = 'STOPPED';
            }
            if (document.getElementById('remoteMotorSpeed')) {
                document.getElementById('remoteMotorSpeed').textContent = '0%';
            }
            if (document.getElementById('remoteLastCommand')) {
                document.getElementById('remoteLastCommand').textContent = 'Motor stopped';
            }
        }).catch(error => {
            console.error('Error stopping motor:', error);
        });
    } else {
        // Start motor
        const speed = document.getElementById('motorSpeedSlider') ? 
                     parseInt(document.getElementById('motorSpeedSlider').value) : 50;
        
        fetch('/api/motor', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ enabled: true, speed: speed })
        }).then(() => {
            button.textContent = 'STOP MOTOR';
            button.style.backgroundColor = '#f44336';
            
            // Update UI
            if (document.getElementById('remoteMotorStatus')) {
                document.getElementById('remoteMotorStatus').textContent = 'RUNNING';
            }
            if (document.getElementById('remoteMotorSpeed')) {
                document.getElementById('remoteMotorSpeed').textContent = speed + '%';
            }
            if (document.getElementById('remoteLastCommand')) {
                document.getElementById('remoteLastCommand').textContent = 'Motor started at ' + speed + '%';
            }
        }).catch(error => {
            console.error('Error starting motor:', error);
        });
    }
}

function emergencyStop() {
    // Stop motor immediately
    fetch('/api/motor', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ enabled: false, speed: 0 })
    }).then(() => {
        // Update UI
        const button = document.getElementById('motorToggle');
        if (button) {
            button.textContent = 'START MOTOR';
            button.style.backgroundColor = '#4caf50';
        }
        
        if (document.getElementById('motorSpeedSlider')) {
            document.getElementById('motorSpeedSlider').value = 0;
        }
        if (document.getElementById('motorSpeedValue')) {
            document.getElementById('motorSpeedValue').textContent = '0';
        }
        if (document.getElementById('remoteMotorStatus')) {
            document.getElementById('remoteMotorStatus').textContent = 'EMERGENCY STOP';
        }
        if (document.getElementById('remoteMotorSpeed')) {
            document.getElementById('remoteMotorSpeed').textContent = '0%';
        }
        if (document.getElementById('remoteLastCommand')) {
            document.getElementById('remoteLastCommand').textContent = 'EMERGENCY STOP activated';
        }
        
        // Show message
        if (document.getElementById('remoteControlStatus')) {
            document.getElementById('remoteControlStatus').textContent = 'Emergency stop activated!';
            setTimeout(() => {
                document.getElementById('remoteControlStatus').textContent = '';
            }, 5000);
        }
    }).catch(error => {
        console.error('Error emergency stopping motor:', error);
    });
}

function toggleLamp() {
    const button = document.getElementById('lampToggleBtn');
    const isOn = button.textContent.includes('OFF') ? false : true;
    
    // New status
    const newStatus = !isOn;
    const newLevel = newStatus ? 1 : 0;
    
    fetch('/api/lamp', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ level: newLevel })
    }).then(() => {
        // Update UI
        button.textContent = newStatus ? '💡 LAMP ON' : '💡 LAMP OFF';
        button.style.backgroundColor = newStatus ? '#ff9800' : '#666';
        
        // Enable/disable brightness controls
        const levelsContainer = document.getElementById('lampLevelsContainer');
        if (levelsContainer) {
            levelsContainer.style.opacity = newStatus ? '1' : '0.5';
        }
        
        // Update status
        if (document.getElementById('remoteLampStatus')) {
            document.getElementById('remoteLampStatus').textContent = newStatus ? 'ON' : 'OFF';
        }
        if (document.getElementById('remoteLastCommand')) {
            document.getElementById('remoteLastCommand').textContent = 'Lamp turned ' + (newStatus ? 'ON' : 'OFF');
        }
    }).catch(error => {
        console.error('Error toggling lamp:', error);
    });
}

// Settings Functions
function saveDPVSettings() {
    console.log('saveDPVSettings called');
    
    // Collect all settings from form
    const form = document.getElementById('settingsForm');
    if (!form) {
        console.error('Settings form not found!');
        alert('Error: Settings form not found!');
        return;
    }
    
    // Create object with all settings
    const settings = {
        // Motor and Speed Settings
        speedSteps: parseInt(document.getElementById('speedSteps').value),
        standbyDelaySeconds: parseInt(document.getElementById('standbyDelaySeconds').value),
        batteryPowerMax: parseInt(document.getElementById('batteryPowerMax').value),
        minSpeedPercent: parseFloat(document.getElementById('minSpeedPercent').value),
        maxSpeedRpm: parseFloat(document.getElementById('maxSpeedRpm').value),
        speedUpTimeMs: parseInt(document.getElementById('speedUpTimeMs').value),
        speedDownTimeMs: parseInt(document.getElementById('speedDownTimeMs').value),
        maxTimeOverloadedMs: parseInt(document.getElementById('maxTimeOverloadedMs').value),
        
        // Jam Detection Settings
        jamMin: parseFloat(document.getElementById('jamMin').value),
        jamDetectionThreshold: parseFloat(document.getElementById('jamDetectionThreshold').value),
        
        // Battery Settings
        cellsInSeries: parseInt(document.getElementById('cellsInSeries').value),
        
        // LED Bar Settings
        ledBarNum: parseInt(document.getElementById('ledBarNum').value),
        ledBarBrightness: parseInt(document.getElementById('ledBarBrightness').value),
        ledBarBrightnessSecond: parseInt(document.getElementById('ledBarBrightnessSecond').value),
        ledFrequency: parseInt(document.getElementById('ledFrequency').value),
        
        // Lamp Settings
        lampMaxLevels: parseInt(document.getElementById('lampMaxLevels').value),
        lampBrightness: [],
        
        // WiFi Settings
        wifiSSID: document.getElementById('wifiSSID').value,
        wifiPassword: document.getElementById('wifiPassword').value,
        
        // System Settings
        beeperEnabled: document.getElementById('beeperEnabled').checked,
        debugLoggingEnabled: document.getElementById('debugLoggingEnabled').checked,
        standbyBlinkStartMinutes: parseInt(document.getElementById('standbyBlinkStartMinutes').value),
        standbyBlinkDurationSeconds: parseInt(document.getElementById('standbyBlinkDurationSeconds').value)
    };
    
    // Initialize lampBrightness array with Level 0 = 0 (OFF)
    settings.lampBrightness[0] = 0;
    
    // Collect lamp brightness values (Level 1 to N)
    const lampContainer = document.getElementById('lampBrightnessContainer');
    if (lampContainer) {
        const lampInputs = lampContainer.querySelectorAll('input[type="range"]');
        for (let i = 0; i < lampInputs.length; i++) {
            // Convert percentage (0-100) to PWM value (0-255)
            const percentageValue = parseInt(lampInputs[i].value);
            const pwmValue = Math.round((percentageValue / 100) * 255);
            // Map input index to correct lampBrightness index (Level 1+)
            settings.lampBrightness[i + 1] = pwmValue;
        }
        // Fill missing values with 0 if fewer than lampMaxLevels values are present
        while (settings.lampBrightness.length < settings.lampMaxLevels) {
            settings.lampBrightness.push(0);
        }
        // Trim excess values if necessary
        settings.lampBrightness = settings.lampBrightness.slice(0, settings.lampMaxLevels);
    } else {
        // Fallback: Fill with default values
        for (let i = 1; i < settings.lampMaxLevels; i++) {
            settings.lampBrightness[i] = 0;
        }
    }
    
    console.log('Saving settings:', settings);
    
    // Show status message
    const statusElement = document.getElementById('settingsStatus');
    if (statusElement) {
        statusElement.textContent = 'Saving settings...';
    }
    
    // Send to API
    fetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(settings)
    }).then(response => {
        if (!response.ok) {
            throw new Error('Server responded with status: ' + response.status);
        }
        return response.json();
    }).then(data => {
        console.log('Settings saved response:', data);
        if (data.success) {
            if (statusElement) {
                statusElement.textContent = 'Settings saved successfully!';
                statusElement.style.color = '#4caf50';
            } else {
                alert('Settings saved successfully!');
            }
        } else {
            if (statusElement) {
                statusElement.textContent = 'Error saving settings: ' + (data.error || 'Unknown error');
                statusElement.style.color = '#f44336';
            } else {
                alert('Error saving settings: ' + (data.error || 'Unknown error'));
            }
        }
        
        // Hide status after 3 seconds
        setTimeout(() => {
            if (statusElement) {
                statusElement.textContent = '';
            }
        }, 3000);
        
    }).catch(error => {
        console.error('Error saving settings:', error);
        if (statusElement) {
            statusElement.textContent = 'Error saving settings: ' + error.message;
            statusElement.style.color = '#f44336';
        } else {
            alert('Error saving settings: ' + error.message);
        }
    });
}

// Function to restore default settings
function restoreDefaultSettings() {
    if (confirm('Are you sure you want to restore default settings? All custom settings will be lost.')) {
        fetch('/api/settings/restore', {
            method: 'POST'
        }).then(response => response.json())
        .then(data => {
            if (data.success) {
                alert('Default settings restored. Reloading...');
                // Reload settings
                loadDPVSettings();
            } else {
                alert('Error restoring default settings: ' + (data.error || 'Unknown error'));
            }
        }).catch(error => {
            console.error('Error restoring default settings:', error);
            alert('Error restoring default settings: ' + error.message);
        });
    }
}

// Function to reboot the system
function rebootSystem() {
    if (confirm('Are you sure you want to reboot the system? This will disconnect you temporarily.')) {
        fetch('/api/reboot', {
            method: 'POST'
        }).then(response => response.json())
        .then(data => {
            if (data.success) {
                alert('System is rebooting. Please wait about 10 seconds and refresh the page.');
                // Show countdown
                const statusElement = document.getElementById('settingsStatus');
                if (statusElement) {
                    let countdown = 10;
                    statusElement.textContent = 'System rebooting... Reconnect in ' + countdown + ' seconds';
                    statusElement.style.color = '#ff9800';
                    
                    const interval = setInterval(() => {
                        countdown--;
                        if (countdown > 0) {
                            statusElement.textContent = 'System rebooting... Reconnect in ' + countdown + ' seconds';
                        } else {
                            clearInterval(interval);
                            statusElement.textContent = 'Attempting to reconnect...';
                            setTimeout(() => {
                                window.location.reload();
                            }, 1000);
                        }
                    }, 1000);
                }
            } else {
                alert('Error rebooting system: ' + (data.error || 'Unknown error'));
            }
        }).catch(error => {
            console.error('Error rebooting system:', error);
            alert('Error rebooting system: ' + error.message);
        });
    }
}

// Function to export settings
function exportSettings() {
    fetch('/api/settings')
        .then(response => response.json())
        .then(settings => {
            // Create JSON file for download
            const dataStr = JSON.stringify(settings, null, 2);
            const dataBlob = new Blob([dataStr], { type: 'application/json' });
            const url = URL.createObjectURL(dataBlob);
            
            // Create download link
            const a = document.createElement('a');
            a.href = url;
            a.download = 'dpv_settings.json';
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            URL.revokeObjectURL(url);
        })
        .catch(error => {
            console.error('Error exporting settings:', error);
            alert('Error exporting settings: ' + error.message);
        });
}

// Function to import settings
function importSettings() {
    // Click on hidden file input
    const fileInput = document.getElementById('settingsFileInput');
    if (fileInput) {
        fileInput.click();
    } else {
        alert('Error: Settings file input not found!');
    }
}

// Function to process imported settings file
function handleSettingsFile(event) {
    const file = event.target.files[0];
    if (!file) return;
    
    const reader = new FileReader();
    reader.onload = function(e) {
        try {
            const settings = JSON.parse(e.target.result);
            
            // Confirm import
            if (confirm('Are you sure you want to import these settings? Current settings will be overwritten.')) {
                // Send imported settings to API
                fetch('/api/settings', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(settings)
                }).then(response => response.json())
                .then(data => {
                    if (data.success) {
                        alert('Settings imported successfully. Reloading...');
                        // Reload settings
                        loadDPVSettings();
                    } else {
                        alert('Error importing settings: ' + (data.error || 'Unknown error'));
                    }
                }).catch(error => {
                    console.error('Error importing settings:', error);
                    alert('Error importing settings: ' + error.message);
                });
            }
        } catch (error) {
            console.error('Error parsing settings file:', error);
            alert('Error parsing settings file: ' + error.message);
        }
    };
    reader.readAsText(file);
}

// Function to update lamp brightness
function updateLampBrightness(level, value) {
    console.log(`Updating lamp brightness for level ${level} to ${value}%`);
    
    // Could call API directly here if desired
    // For now we just store the value, which is transmitted when saving settings
}

// Function to dynamically generate lamp level selection
function generateLampLevels() {
    console.log('Generating lamp level controls...');
    
    // Get settings from server
    fetch('/api/settings')
        .then(response => response.json())
        .then(settings => {
            const maxLevels = settings.lampMaxLevels || 5;
            const container = document.getElementById('lampLevelRadios');
            if (!container) return;
            
            container.innerHTML = '';
            
            // Remove Level 0 (Off) radio button, as ON/OFF is handled by toggle button
            // Generate only configured brightness levels (starting from 1)
            for (let i = 1; i < maxLevels; i++) {
                const levelDiv = document.createElement('div');
                levelDiv.className = 'lamp-level-option';
                
                const radio = document.createElement('input');
                radio.type = 'radio';
                radio.name = 'lampLevel';
                radio.id = 'lampLevel' + i;
                radio.value = i.toString();
                radio.onchange = () => setLampLevel(i);
                
                const label = document.createElement('label');
                label.htmlFor = 'lampLevel' + i;
                
                // Convert PWM value (0-255) to percentage (0-100%) for display
                const pwmValue = settings.lampBrightness[i] || 0;
                const percentageValue = Math.round((pwmValue / 255) * 100);
                label.textContent = `Level ${i} (${percentageValue}%)`;
                
                levelDiv.appendChild(radio);
                levelDiv.appendChild(label);
                container.appendChild(levelDiv);
            }
            
            console.log(`Generated ${maxLevels - 1} lamp level options`);
        })
        .catch(error => {
            console.error('Error loading lamp levels:', error);
        });
}

// Function to set lamp level
function setLampLevel(level) {
    console.log(`Setting lamp level to ${level}`);
    
    fetch('/api/lamp', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ level: parseInt(level) })
    }).then(() => {
        // Update UI
        if (document.getElementById('remoteLampStatus')) {
            document.getElementById('remoteLampStatus').textContent = level > 0 ? 'ON (Level ' + level + ')' : 'OFF';
        }
        if (document.getElementById('remoteLastCommand')) {
            document.getElementById('remoteLastCommand').textContent = 'Lamp set to level ' + level;
        }
        
        // Update button
        const button = document.getElementById('lampToggleBtn');
        if (button) {
            if (level > 0) {
                button.textContent = '💡 LAMP ON';
                button.style.backgroundColor = '#ff9800';
                
                // Enable brightness controls
                const levelsContainer = document.getElementById('lampLevelsContainer');
                if (levelsContainer) {
                    levelsContainer.style.opacity = '1';
                }
            } else {
                button.textContent = '💡 LAMP OFF';
                button.style.backgroundColor = '#666';
                
                // Disable brightness controls
                const levelsContainer = document.getElementById('lampLevelsContainer');
                if (levelsContainer) {
                    levelsContainer.style.opacity = '0.5';
                }
            }
        }
    }).catch(error => {
        console.error('Error setting lamp level:', error);
    });
}

// Data filter categories for quick filtering
const dataCategories = {
    main: [
        'Battery Voltage (V)',
        'Current (A)', 
        'Motor Temp (°C)',
        'Ambient Temp (°C)',
        'Humidity (%)',
        'eRPM',
        'Duty Cycle (%)',
        'MOSFET Temp (°C)',
        'Motor Current (A)',
        'Battery Level (%)',
        'LED Brightness (%)'
    ],
    boolean: [
        'Beeper Active',
        'Left Button',
        'Right Button', 
        'Leak Sensor'
    ]
};

// Apply data filter to show/hide chart datasets
function applyDataFilter(filterType) {
    if (!charts.combinedChart) return;
    
    const chart = charts.combinedChart;
    const datasets = chart.data.datasets;
    
    // Define which datasets to show based on filter type
    let datasetsToShow = [];
    
    switch(filterType) {
        case 'main':
            datasetsToShow = dataCategories.main;
            break;
        case 'boolean':
            datasetsToShow = dataCategories.boolean;
            break;
        case 'all':
            datasetsToShow = [...dataCategories.main, ...dataCategories.boolean];
            break;
        case 'none':
            datasetsToShow = [];
            break;
        default:
            console.warn('Unknown filter type:', filterType);
            return;
    }
    
    // Update dataset visibility
    datasets.forEach(dataset => {
        const shouldShow = datasetsToShow.includes(dataset.label);
        
        // Use Chart.js built-in visibility toggle
        const meta = chart.getDatasetMeta(datasets.indexOf(dataset));
        if (meta) {
            meta.hidden = !shouldShow;
        }
    });
    
    // Update the chart
    chart.update('none'); // No animation for better performance
    
    console.log(`Applied filter: ${filterType}, showing ${datasetsToShow.length} datasets`);
} 