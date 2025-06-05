# PowerShell-Skript zum Anlegen des pre-commit-Hooks für Git unter Windows
$hookPath = ".git/hooks/pre-commit"
$ps1Path = "pre-commit.ps1"
$pwshPath = "C:\Program Files\PowerShell\7\pwsh.exe"

$batchContent = @"
@echo off
REM pre-commit hook wrapper for PowerShell script
"$pwshPath" -NoProfile -ExecutionPolicy Bypass -File "%~dp0\..\..\$ps1Path"
exit /b %ERRORLEVEL%
"@

# Schreibe die Datei als UTF-8 mit CRLF
[System.IO.File]::WriteAllText($hookPath, $batchContent, [System.Text.Encoding]::UTF8)
Write-Host "pre-commit-Hook wurde erfolgreich erstellt: $hookPath" 