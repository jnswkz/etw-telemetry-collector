# Gỡ ETW Telemetry Collector Windows Service. Cần quyền Administrator.
param(
    [string]$ExePath = "$PSScriptRoot\..\build\x64-release\bin\etwcollector.exe"
)

if (Get-Service -Name "EtwTelemetryCollector" -ErrorAction SilentlyContinue) {
    Stop-Service -Name "EtwTelemetryCollector" -ErrorAction SilentlyContinue
}

& $ExePath --uninstall
if ($LASTEXITCODE -eq 0) {
    Write-Host "Đã gỡ service." -ForegroundColor Green
} else {
    Write-Error "Gỡ service thất bại (mã $LASTEXITCODE)."
}
