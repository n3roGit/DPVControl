# DPV Control - Test Runner Script
# Runs all tests for the DPV Control project

Write-Host "===============================================" -ForegroundColor Cyan
Write-Host "  DPV Control - Running All Tests" -ForegroundColor Cyan
Write-Host "===============================================" -ForegroundColor Cyan

$ErrorActionPreference = "Stop"
$startTime = Get-Date

try {
    # Run native tests (API, integration, validation tests)
    Write-Host "`nRunning native tests..." -ForegroundColor Yellow
    Write-Host "Tests include:" -ForegroundColor Gray
    Write-Host "  - API endpoint validation" -ForegroundColor Gray
    Write-Host "  - Status data uptime fields (prevents missing uptime bug)" -ForegroundColor Gray
    Write-Host "  - JSON response validation" -ForegroundColor Gray
    Write-Host "  - Required field presence checks" -ForegroundColor Gray
    
    python -m platformio test -e native -v
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "❌ Native tests failed!" -ForegroundColor Red
        exit 1
    }
    
    Write-Host "✅ Native tests passed!" -ForegroundColor Green
    
    # Compile firmware for ESP32 (no tests, just verify compilation)
    Write-Host "`nCompiling ESP32 firmware..." -ForegroundColor Yellow
    python -m platformio run -e esp32dev
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "❌ ESP32 compilation failed!" -ForegroundColor Red
        exit 1
    }
    
    Write-Host "✅ ESP32 compilation successful!" -ForegroundColor Green
    
    # Calculate elapsed time
    $endTime = Get-Date
    $elapsed = $endTime - $startTime
    
    Write-Host "`n===============================================" -ForegroundColor Cyan
    Write-Host "  All Tests Completed Successfully! ✅" -ForegroundColor Green
    Write-Host "  Time elapsed: $($elapsed.ToString('mm\:ss'))" -ForegroundColor Cyan
    Write-Host "===============================================" -ForegroundColor Cyan
    
    Write-Host "`nTest Summary:" -ForegroundColor White
    Write-Host "  ✅ API Tests: All endpoints return correct JSON" -ForegroundColor Green
    Write-Host "  ✅ Status Tests: Uptime fields present (bug prevention)" -ForegroundColor Green
    Write-Host "  ✅ Validation Tests: Required fields verified" -ForegroundColor Green
    Write-Host "  ✅ ESP32 Compilation: Firmware builds without errors" -ForegroundColor Green
    
    Write-Host "`nThese tests help prevent issues like:" -ForegroundColor Yellow
    Write-Host "  - Missing uptime/totalUptime in status API" -ForegroundColor Gray
    Write-Host "  - Invalid JSON responses from API endpoints" -ForegroundColor Gray
    Write-Host "  - Battery level calculation errors" -ForegroundColor Gray
    Write-Host "  - Settings validation bypasses" -ForegroundColor Gray
    
}
catch {
    Write-Host "❌ Error running tests: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
} 