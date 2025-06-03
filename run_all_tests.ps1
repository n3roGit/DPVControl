# run_all_tests.ps1
# This script runs all tests by iterating over each test file in the test directory,
# updating the test_filter in platformio.ini, and executing the tests.
# It reports the overall result at the end.

$platformioIniPath = "platformio.ini"
$testDir = "test"

# Read the current platformio.ini content
$iniContent = Get-Content -Path $platformioIniPath -Raw

# Find all test files in the test directory
$testFiles = Get-ChildItem -Path $testDir -Filter "test_*.cpp" | ForEach-Object { $_.BaseName }

$allTestsPassed = $true

foreach ($testFile in $testFiles) {
    Write-Host "Running tests for $testFile..."
    
    # Update the test_filter in platformio.ini
    $updatedContent = $iniContent -replace "test_filter = .*", "test_filter = $testFile"
    $updatedContent | Set-Content -Path $platformioIniPath
    
    # Run the tests
    $result = python -m platformio test -e native
    
    # Check if the tests passed
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Tests for $testFile failed."
        $allTestsPassed = $false
    }
    else {
        Write-Host "Tests for $testFile passed."
    }
}

# Report overall result
if ($allTestsPassed) {
    Write-Host "All tests passed successfully!"
}
else {
    Write-Host "Some tests failed. Please check the output above."
} 