# Build ETW Telemetry Collector từ BẤT KỲ PowerShell nào.
# Tự động: tìm Visual Studio -> vào môi trường dev (cl.exe + ninja) ->
# set VCPKG_ROOT (vcpkg bundled trong VS nếu chưa set) -> cmake preset.
#
# Dùng:  .\scripts\build.ps1               (mặc định x64-release)
#        .\scripts\build.ps1 x64-debug
param([string]$Preset = "x64-release")

$ErrorActionPreference = "Stop"

# 1. Định vị Visual Studio qua vswhere.
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    $vswhere = "$env:ProgramFiles\Microsoft Visual Studio\Installer\vswhere.exe"
}
if (-not (Test-Path $vswhere)) {
    throw "Không tìm thấy vswhere.exe. Cần cài Visual Studio 2022+ với C++ toolset."
}

$vsPath = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if (-not $vsPath) {
    throw "Không tìm thấy Visual Studio có MSVC (VC++ x64) toolchain."
}
Write-Host "Visual Studio: $vsPath"

# 2. Vào Developer Shell -> đưa cl.exe, ninja... vào PATH của phiên này.
$devShell = Join-Path $vsPath "Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Import-Module $devShell
Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation `
    -DevCmdArguments "-arch=x64 -host_arch=x64" | Out-Null

# 3. VCPKG_ROOT: giữ nếu đã set, ngược lại dùng vcpkg bundled trong VS.
if (-not $env:VCPKG_ROOT) {
    $env:VCPKG_ROOT = Join-Path $vsPath "VC\vcpkg"
}
Write-Host "VCPKG_ROOT: $env:VCPKG_ROOT"

if (-not (Test-Path (Join-Path $env:VCPKG_ROOT "scripts\buildsystems\vcpkg.cmake"))) {
    throw "VCPKG_ROOT không hợp lệ (thiếu vcpkg.cmake): $env:VCPKG_ROOT"
}

# 4. Configure + build (chạy tại thư mục gốc dự án).
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
Push-Location $root
try {
    cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw "cmake configure thất bại ($LASTEXITCODE)" }
    cmake --build --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw "cmake build thất bại ($LASTEXITCODE)" }
    Write-Host "`nBuild xong. Binary: build\$Preset\bin\etwcollector.exe" -ForegroundColor Green
}
finally {
    Pop-Location
}
