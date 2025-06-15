#!/usr/bin/env pwsh
# Local C++ linting script - runs the same checks as GitHub Actions

Write-Host "Running C++ linting checks..."

$errors = 0

# Check if clang-format is available
if (-not (Get-Command clang-format -ErrorAction SilentlyContinue)) {
    Write-Host "WARNING: clang-format not found. Install LLVM/Clang tools." -ForegroundColor Yellow
}
else {
    Write-Host "Checking code formatting with clang-format..."
    $formatFiles = Get-ChildItem -Recurse -Path src, test -Include *.cpp, *.h, *.hpp
    foreach ($file in $formatFiles) {
        $result = & clang-format --dry-run --Werror --style=file $file.FullName 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Host "ERROR: Formatting issues in $($file.Name)" -ForegroundColor Red
            $errors++
        }
    }
}

# Check if clang-tidy is available  
if (-not (Get-Command clang-tidy -ErrorAction SilentlyContinue)) {
    Write-Host "WARNING: clang-tidy not found. Install LLVM/Clang tools." -ForegroundColor Yellow
}
else {
    Write-Host "Running clang-tidy analysis..."
    $cppFiles = Get-ChildItem -Recurse -Path src -Include *.cpp
    foreach ($file in $cppFiles) {
        & clang-tidy --config-file=.clang-tidy $file.FullName 2>&1 | Where-Object { $_ -match "error:" }
        if ($LASTEXITCODE -ne 0) {
            $errors++
        }
    }
}

# Check if cppcheck is available
if (-not (Get-Command cppcheck -ErrorAction SilentlyContinue)) {
    Write-Host "WARNING: cppcheck not found. Install cppcheck." -ForegroundColor Yellow
}
else {
    Write-Host "Running cppcheck static analysis..."
    & cppcheck --project=check.cppcheck --error-exitcode=1 --enable=warning, style, performance, portability --suppress=missingIncludeSystem --suppress=unmatchedSuppression --inline-suppr src/ 2>&1
    if ($LASTEXITCODE -ne 0) {
        $errors++
    }
}

# Check if cpplint is available
if (-not (Get-Command cpplint -ErrorAction SilentlyContinue)) {
    Write-Host "WARNING: cpplint not found. Install with: pip install cpplint" -ForegroundColor Yellow
}
else {
    Write-Host "Running cpplint style check..."
    $lintFiles = Get-ChildItem -Recurse -Path src -Include *.cpp, *.h
    foreach ($file in $lintFiles) {
        & cpplint --config=.cpplint $file.FullName 2>&1
        if ($LASTEXITCODE -ne 0) {
            $errors++
        }
    }
}

if ($errors -eq 0) {
    Write-Host "All linting checks passed!" -ForegroundColor Green
    exit 0
}
else {
    Write-Host "$errors linting errors found!" -ForegroundColor Red
    exit 1
} 