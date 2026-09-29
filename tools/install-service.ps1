# Cài đặt ETW Telemetry Collector như một Windows Service.
# Chạy PowerShell bằng quyền Administrator.
param(
    [string]$ExePath = "$PSScriptRoot\..\build\x64-release\bin\etwcollector.exe"
)

if (-not (Test-Path $ExePath)) {
    Write-Error "Không tìm thấy binary: $ExePath. Hãy build trước."
    exit 1
}

& $ExePath --install
if ($LASTEXITCODE -eq 0) {
    Write-Host "Đã cài service EtwTelemetryCollector." -ForegroundColor Green
    Start-Service -Name "EtwTelemetryCollector"
    Get-Service -Name "EtwTelemetryCollector"
} else {
    Write-Error "Cài service thất bại (mã $LASTEXITCODE)."
}
