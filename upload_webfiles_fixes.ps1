#!/usr/bin/env pwsh

Write-Host "🔧 Uploading Fixed Web Files to DPVControl ESP32" -ForegroundColor Cyan
Write-Host "=" * 60

$ESP32_IP = "4.3.2.1"
$LOCAL_SERVER = "http://127.0.0.1:8080"

Write-Host "📡 ESP32 IP: $ESP32_IP" -ForegroundColor Yellow
Write-Host "🌐 Local Server: $LOCAL_SERVER" -ForegroundColor Yellow

# Test ESP32 connectivity
Write-Host "`n🔍 Testing ESP32 connectivity..." -ForegroundColor Cyan
try {
    $response = Invoke-WebRequest -Uri "http://$ESP32_IP/api/status" -TimeoutSec 5 -UseBasicParsing
    Write-Host "✅ ESP32 is reachable" -ForegroundColor Green
}
catch {
    Write-Host "❌ ESP32 not reachable at $ESP32_IP" -ForegroundColor Red
    Write-Host "Please check ESP32 connection and IP address." -ForegroundColor Yellow
    exit 1
}

# Files to upload (only the fixed ones)
$files = @(
    "app.js"
)

Write-Host "`n📁 Files to upload:" -ForegroundColor Cyan
foreach ($file in $files) {
    $size = (Get-Item "data/$file").Length
    Write-Host "  📄 $file ($([math]::Round($size/1KB, 1)) KB)" -ForegroundColor White
}

Write-Host "`n⚠️  IMPORTANT NOTICE:" -ForegroundColor Yellow
Write-Host "This will update the web interface files on ESP32." -ForegroundColor Yellow
Write-Host "All runtime data (sessions, settings, uptime) will be PRESERVED!" -ForegroundColor Green

do {
    $response = Read-Host "`n🤔 Continue with upload? (Y/N)"
    $response = $response.ToUpper()
} while ($response -ne "Y" -and $response -ne "N")

if ($response -eq "N") {
    Write-Host "❌ Upload cancelled by user." -ForegroundColor Red
    exit 0
}

Write-Host "`n🚀 Starting file upload..." -ForegroundColor Cyan

$successCount = 0
$totalFiles = $files.Count

foreach ($file in $files) {
    Write-Host "`n📤 Uploading $file..." -ForegroundColor Yellow
    
    try {
        # Upload file to ESP32
        $uploadUrl = "http://$ESP32_IP/upload"
        $localFileUrl = "$LOCAL_SERVER/$file"
        
        # Create a simple HTML form to upload the file
        $boundary = [System.Guid]::NewGuid().ToString()
        $fileContent = Invoke-WebRequest -Uri $localFileUrl -UseBasicParsing
        
        $bodyTemplate = @"
--$boundary
Content-Disposition: form-data; name="file"; filename="$file"
Content-Type: application/octet-stream

{0}
--$boundary--
"@
        
        $body = $bodyTemplate -f [System.Text.Encoding]::UTF8.GetString($fileContent.Content)
        
        $headers = @{
            "Content-Type" = "multipart/form-data; boundary=$boundary"
        }
        
        $response = Invoke-WebRequest -Uri $uploadUrl -Method Post -Body $body -Headers $headers -TimeoutSec 30 -UseBasicParsing
        
        if ($response.StatusCode -eq 200) {
            Write-Host "✅ $file uploaded successfully" -ForegroundColor Green
            $successCount++
        }
        else {
            Write-Host "❌ Failed to upload $file (Status: $($response.StatusCode))" -ForegroundColor Red
        }
        
    }
    catch {
        Write-Host "❌ Error uploading $file`: $($_.Exception.Message)" -ForegroundColor Red
    }
}

Write-Host "`n📊 Upload Summary:" -ForegroundColor Cyan
Write-Host "✅ Successfully uploaded: $successCount/$totalFiles files" -ForegroundColor $(if ($successCount -eq $totalFiles) { "Green" } else { "Yellow" })

if ($successCount -eq $totalFiles) {
    Write-Host "`n🎉 All fixes uploaded successfully!" -ForegroundColor Green
    Write-Host "🔄 Please refresh your browser to see the changes." -ForegroundColor Yellow
    Write-Host "🐛 CSV export and time window should now work correctly." -ForegroundColor Cyan
}
else {
    Write-Host "`n⚠️  Some files failed to upload. Check ESP32 status." -ForegroundColor Yellow
}

Write-Host "`n🔚 Upload process completed." -ForegroundColor Cyan 