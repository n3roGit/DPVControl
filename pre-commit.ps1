#!/usr/bin/env pwsh
# pre-commit hook: Führt alle Tests aus und verhindert Commit bei Fehlern

Write-Host "Running all tests before commit..."

$testResult = & ./run_all_tests.ps1

if ($LASTEXITCODE -ne 0) {
    Write-Host "\nERROR: Some tests failed. Commit aborted!"
    exit 1
}

Write-Host "All tests passed. Commit allowed."
exit 0 