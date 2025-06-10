#!/usr/bin/env pwsh

# DPVControl Test Runner
# Runs all tests and provides comprehensive reporting

Write-Host "🧪 DPVControl Test Suite Runner" -ForegroundColor Cyan
Write-Host "=================================" -ForegroundColor Cyan
Write-Host ""

# Track test results
$totalTests = 0
$passedTests = 0
$failedTests = 0
$testResults = @()

function Run-TestSuite {
    param(
        [string]$TestName,
        [string]$Command
    )
    
    Write-Host "🔹 Running $TestName..." -ForegroundColor Yellow
    Write-Host "Command: $Command" -ForegroundColor Gray
    Write-Host ""
    
    $startTime = Get-Date
    
    try {
        $result = Invoke-Expression $Command 2>&1
        $exitCode = $LASTEXITCODE
        
        $endTime = Get-Date
        $duration = ($endTime - $startTime).TotalSeconds
        
        if ($exitCode -eq 0) {
            Write-Host "✅ $TestName PASSED (${duration}s)" -ForegroundColor Green
            $script:passedTests++
            
            # Extract test count from output
            $testCount = 0
            $resultText = ($result | Out-String)
            if ($resultText -match "(\d+) test cases?: (\d+) succeeded") {
                $testCount = [int]$matches[2]
            }
            elseif ($resultText -match "(\d+) Tests (\d+) Failures (\d+) Ignored") {
                $testCount = [int]$matches[1]
            }
            
            $script:testResults += [PSCustomObject]@{
                Suite     = $TestName
                Status    = "PASSED"
                Duration  = $duration
                TestCount = $testCount
                Details   = ($result | Out-String)
            }
        }
        else {
            Write-Host "❌ $TestName FAILED (exit code: $exitCode)" -ForegroundColor Red
            $script:failedTests++
            
            $script:testResults += [PSCustomObject]@{
                Suite     = $TestName
                Status    = "FAILED"
                Duration  = $duration
                TestCount = 0
                Details   = ($result | Out-String)
                Error     = "Exit code: $exitCode"
            }
        }
    }
    catch {
        Write-Host "💥 $TestName CRASHED: $($_.Exception.Message)" -ForegroundColor Red
        $script:failedTests++
        
        $script:testResults += [PSCustomObject]@{
            Suite     = $TestName
            Status    = "CRASHED"
            Duration  = 0
            TestCount = 0
            Details   = ""
            Error     = $_.Exception.Message
        }
    }
    
    $script:totalTests++
    Write-Host ""
}

# Run all test suites
Write-Host "Starting test execution..." -ForegroundColor White
Write-Host ""

# 1. API Tests
Run-TestSuite "API Tests" "python -m platformio test -e native -f test_api"

# 2. Compilation Test (ESP32)
Run-TestSuite "ESP32 Compilation" "python -m platformio run -e esp32dev"

# Summary Report
Write-Host "📊 TEST SUMMARY" -ForegroundColor Cyan
Write-Host "===============" -ForegroundColor Cyan
Write-Host ""

$totalTestCases = ($testResults | Measure-Object -Property TestCount -Sum).Sum
$successRate = if ($totalTests -gt 0) { [math]::Round(($passedTests / $totalTests) * 100, 1) } else { 0 }

Write-Host "Test Suites:   $totalTests total" -ForegroundColor White
Write-Host "Passed:        $passedTests" -ForegroundColor Green
Write-Host "Failed:        $failedTests" -ForegroundColor Red
Write-Host "Success Rate:  $successRate%" -ForegroundColor $(if ($successRate -eq 100) { "Green" } else { "Yellow" })
Write-Host "Test Cases:    $totalTestCases individual tests" -ForegroundColor White
Write-Host ""

# Detailed Results
foreach ($result in $testResults) {
    $statusColor = switch ($result.Status) {
        "PASSED" { "Green" }
        "FAILED" { "Red" }
        "CRASHED" { "Magenta" }
    }
    
    Write-Host "📋 $($result.Suite): " -NoNewline
    Write-Host "$($result.Status)" -ForegroundColor $statusColor -NoNewline
    Write-Host " ($($result.Duration.ToString('F1'))s, $($result.TestCount) tests)" -ForegroundColor Gray
    
    if ($result.Status -ne "PASSED") {
        Write-Host "   Error: $($result.Error)" -ForegroundColor Red
        
        # Show relevant error details
        $lines = $result.Details -split "`n"
        $errorLines = $lines | Where-Object { 
            $_ -match "(FAIL|ERROR|ASSERT)" -or 
            $_ -match "test.*\[FAILED\]" -or
            $_ -match "Expected.*Actual"
        }
        
        if ($errorLines) {
            Write-Host "   Details:" -ForegroundColor Yellow
            foreach ($line in $errorLines | Select-Object -First 3) {
                Write-Host "     $line" -ForegroundColor Yellow
            }
        }
    }
}

Write-Host ""

# Quality Gates
Write-Host "🎯 QUALITY GATES" -ForegroundColor Cyan
Write-Host "=================" -ForegroundColor Cyan

$qualityChecks = @(
    @{ Name = "All tests pass"; Condition = ($failedTests -eq 0); Critical = $true },
    @{ Name = "Compilation successful"; Condition = ($testResults | Where-Object { $_.Suite -eq "ESP32 Compilation" -and $_.Status -eq "PASSED" }) -ne $null; Critical = $true },
    @{ Name = "API coverage complete"; Condition = ($testResults | Where-Object { $_.Suite -eq "API Tests" -and $_.TestCount -ge 9 }) -ne $null; Critical = $false }
)

$criticalFailures = 0

foreach ($check in $qualityChecks) {
    $passed = $check.Condition
    $symbol = if ($passed) { "✅" } else { if ($check.Critical) { "🚫" } else { "⚠️" } }
    $color = if ($passed) { "Green" } else { if ($check.Critical) { "Red" } else { "Yellow" } }
    
    Write-Host "$symbol $($check.Name)" -ForegroundColor $color
    
    if (-not $passed -and $check.Critical) {
        $criticalFailures++
    }
}

Write-Host ""

# Final Result
if ($criticalFailures -eq 0 -and $failedTests -eq 0) {
    Write-Host "🎉 ALL TESTS PASSED! Ready for deployment." -ForegroundColor Green
    $exitCode = 0
}
elseif ($criticalFailures -eq 0) {
    Write-Host "⚠️  Some non-critical issues found, but build is OK." -ForegroundColor Yellow
    $exitCode = 0
}
else {
    Write-Host "🛑 CRITICAL FAILURES DETECTED! Fix before proceeding." -ForegroundColor Red
    $exitCode = 1
}

Write-Host ""
Write-Host "Test run completed. Check results above." -ForegroundColor White

# Generate test report file
$timestamp = Get-Date -Format "yyyy-MM-dd_HH-mm-ss"
$reportFile = "test_report_$timestamp.txt"

$report = @"
DPVControl Test Report
Generated: $(Get-Date)
======================

SUMMARY:
- Test Suites: $totalTests total, $passedTests passed, $failedTests failed
- Success Rate: $successRate%
- Test Cases: $totalTestCases individual tests

DETAILED RESULTS:
$($testResults | Format-Table -AutoSize | Out-String)

QUALITY GATES:
$(foreach ($check in $qualityChecks) { "$(if ($check.Condition) { 'PASS' } else { 'FAIL' }): $($check.Name)" })

OVERALL RESULT: $(if ($exitCode -eq 0) { 'PASSED' } else { 'FAILED' })
"@

$report | Out-File -FilePath $reportFile -Encoding UTF8
Write-Host "📄 Detailed report saved to: $reportFile" -ForegroundColor Cyan

exit $exitCode 