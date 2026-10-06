# Gỡ ETW Telemetry Collector. Tương thích Windows PowerShell 5.1. Cần quyền Administrator.
#
# Dùng:  .\uninstall-service.ps1               (giữ %ProgramData%\EtwCollector để phân tích)
#        .\uninstall-service.ps1 -RemoveData   (xóa luôn DB + log)
param(
    [string]$InstallDir = "$env:ProgramFiles\EtwCollector",
    [switch]$RemoveData
)

$ErrorActionPreference = "Stop"
$ServiceName = "EtwTelemetryCollector"
$DataRoot = Join-Path $env:ProgramData "EtwCollector"

$principal = New-Object Security.Principal.WindowsPrincipal(
    [Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw "Cần chạy PowerShell bằng quyền Administrator."
}

# 1. Dừng + xóa service.
$svc = Get-Service -Name $ServiceName -ErrorAction SilentlyContinue
if ($svc) {
    if ($svc.Status -ne "Stopped") {
        Stop-Service -Name $ServiceName -Force
        $svc.WaitForStatus("Stopped", [TimeSpan]::FromSeconds(30))
    }
    & sc.exe delete $ServiceName | Out-Null
    Write-Host "Đã gỡ service $ServiceName." -ForegroundColor Green
} else {
    Write-Host "Không có service $ServiceName."
}

# 2. Dọn ETW session còn sót (vd. sau khi tiến trình bị kill).
& logman.exe query $ServiceName -ets *> $null
if ($LASTEXITCODE -eq 0) {
    & logman.exe stop $ServiceName -ets | Out-Null
    Write-Host "Đã dừng ETW session còn sót."
}

# 3. Xóa binary (script đang chạy từ InstallDir thì vẫn xóa được trên NTFS sau khi nạp).
if (Test-Path $InstallDir) {
    Remove-Item -Recurse -Force $InstallDir
    Write-Host "Đã xóa $InstallDir."
}

# 4. Dữ liệu.
if ($RemoveData) {
    if (Test-Path $DataRoot) {
        Remove-Item -Recurse -Force $DataRoot
        Write-Host "Đã xóa dữ liệu $DataRoot."
    }
} elseif (Test-Path $DataRoot) {
    Write-Host "Giữ dữ liệu tại $DataRoot (dùng -RemoveData để xóa)."
}
