# Cài (hoặc nâng cấp) ETW Telemetry Collector thành Windows Service.
# Tương thích Windows PowerShell 5.1 (máy sạch). Cần quyền Administrator.
#
# Bố cục sau khi cài:
#   <InstallDir>\etwcollector.exe, <InstallDir>\config\collector.json   (chỉ đọc)
#   %ProgramData%\EtwCollector\data\, ...\logs\                        (SYSTEM + Administrators)
#
# Dùng:  .\install-service.ps1                 (từ gói ZIP hoặc từ repo sau khi build)
#        .\install-service.ps1 -ForceConfig    (ghi đè config đang có khi nâng cấp)
#        .\install-service.ps1 -NoStart
param(
    [string]$SourceDir = "",
    [string]$InstallDir = "$env:ProgramFiles\EtwCollector",
    [switch]$ForceConfig,
    [switch]$NoStart
)

$ErrorActionPreference = "Stop"
$ServiceName = "EtwTelemetryCollector"
$DataRoot = Join-Path $env:ProgramData "EtwCollector"

$principal = New-Object Security.Principal.WindowsPrincipal(
    [Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw "Cần chạy PowerShell bằng quyền Administrator."
}

# 1. Tìm nguồn: gói ZIP (exe cạnh script) hoặc repo (build\x64-release\bin).
if (-not $SourceDir) {
    if (Test-Path (Join-Path $PSScriptRoot "etwcollector.exe")) {
        $SourceDir = $PSScriptRoot
    } else {
        $SourceDir = Join-Path $PSScriptRoot "..\build\x64-release\bin"
    }
}
$srcExe = Join-Path $SourceDir "etwcollector.exe"
if (-not (Test-Path $srcExe)) {
    throw "Không tìm thấy binary: $srcExe. Hãy build hoặc giải nén gói trước."
}
$srcConfig = Join-Path $SourceDir "config\collector.json"
if (-not (Test-Path $srcConfig)) {
    $srcConfig = Join-Path $PSScriptRoot "..\config\collector.json"
}

# 2. Nâng cấp: dừng + gỡ service cũ (dữ liệu giữ nguyên).
$existing = Get-Service -Name $ServiceName -ErrorAction SilentlyContinue
if ($existing) {
    Write-Host "Đã có service $ServiceName -> nâng cấp."
    if ($existing.Status -ne "Stopped") {
        Stop-Service -Name $ServiceName -Force
        $existing.WaitForStatus("Stopped", [TimeSpan]::FromSeconds(30))
    }
    & sc.exe delete $ServiceName | Out-Null
    for ($i = 0; $i -lt 20 -and (Get-Service $ServiceName -ErrorAction SilentlyContinue); $i++) {
        Start-Sleep -Milliseconds 250
    }
}

# 3. Copy binary + config.
New-Item -ItemType Directory -Force -Path (Join-Path $InstallDir "config") | Out-Null
Copy-Item $srcExe (Join-Path $InstallDir "etwcollector.exe") -Force
$dstConfig = Join-Path $InstallDir "config\collector.json"
if ((Test-Path $srcConfig) -and ($ForceConfig -or -not (Test-Path $dstConfig))) {
    Copy-Item $srcConfig $dstConfig -Force
} elseif (Test-Path $dstConfig) {
    Write-Host "Giữ config hiện có: $dstConfig (dùng -ForceConfig để ghi đè)."
}
foreach ($f in @("install-service.ps1", "uninstall-service.ps1")) {
    $p = Join-Path $PSScriptRoot $f
    if (Test-Path $p) { Copy-Item $p (Join-Path $InstallDir $f) -Force }
}

# 4. Thư mục dữ liệu + ACL: chỉ SYSTEM và Administrators (command line có thể nhạy cảm).
foreach ($d in @("data", "logs")) {
    New-Item -ItemType Directory -Force -Path (Join-Path $DataRoot $d) | Out-Null
}
& icacls.exe $DataRoot /inheritance:r /grant:r "*S-1-5-18:(OI)(CI)F" "*S-1-5-32-544:(OI)(CI)F" /T /Q | Out-Null
if ($LASTEXITCODE -ne 0) { throw "Đặt ACL cho $DataRoot thất bại ($LASTEXITCODE)." }

# 5. Đăng ký service (exe tự ghi đường dẫn có ngoặc kép, LocalSystem, auto-start, recovery).
$exe = Join-Path $InstallDir "etwcollector.exe"
& $exe --install
if ($LASTEXITCODE -ne 0) { throw "Đăng ký service thất bại (mã $LASTEXITCODE)." }
Write-Host "Đã cài service $ServiceName tại $InstallDir" -ForegroundColor Green

if (-not $NoStart) {
    Start-Service -Name $ServiceName
    (Get-Service $ServiceName).WaitForStatus("Running", [TimeSpan]::FromSeconds(15))
    Start-Sleep -Seconds 2
    Get-Service -Name $ServiceName | Format-Table -AutoSize Name, Status, StartType
    $log = Join-Path $DataRoot "logs\collector.log"
    if (Test-Path $log) {
        Write-Host "--- $log (10 dòng cuối) ---"
        Get-Content $log -Tail 10
    }
}
Write-Host "Dữ liệu: $DataRoot"
