#!/usr/bin/env pwsh
# Installation script for C++ linting tools

Write-Host "Installing C++ linting tools..." -ForegroundColor Green

# Install LLVM/Clang (includes clang-format and clang-tidy)
if (-not (Get-Command clang-format -ErrorAction SilentlyContinue)) {
    Write-Host "Installing LLVM/Clang tools..."
    
    if ($IsWindows) {
        Write-Host "Please install LLVM from: https://github.com/llvm/llvm-project/releases"
        Write-Host "Or use Chocolatey: choco install llvm"
        Write-Host "Or use winget: winget install LLVM.LLVM"
    }
    elseif ($IsLinux) {
        Write-Host "Installing via apt..."
        sudo apt-get update
        sudo apt-get install -y clang-format clang-tidy
    }
    elseif ($IsMacOS) {
        Write-Host "Installing via Homebrew..."
        brew install llvm
    }
}
else {
    Write-Host "✓ LLVM/Clang tools already installed" -ForegroundColor Green
}

# Install cppcheck
if (-not (Get-Command cppcheck -ErrorAction SilentlyContinue)) {
    Write-Host "Installing cppcheck..."
    
    if ($IsWindows) {
        Write-Host "Please install cppcheck from: https://cppcheck.sourceforge.io/"
        Write-Host "Or use Chocolatey: choco install cppcheck"
    }
    elseif ($IsLinux) {
        sudo apt-get install -y cppcheck
    }
    elseif ($IsMacOS) {
        brew install cppcheck
    }
}
else {
    Write-Host "✓ cppcheck already installed" -ForegroundColor Green
}

# Install cpplint
if (-not (Get-Command cpplint -ErrorAction SilentlyContinue)) {
    Write-Host "Installing cpplint..."
    pip install cpplint
}
else {
    Write-Host "✓ cpplint already installed" -ForegroundColor Green
}

Write-Host "Installation complete!" -ForegroundColor Green
Write-Host "You can now run: ./lint-check.ps1" -ForegroundColor Yellow 