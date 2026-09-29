# Build nhanh bằng CMake preset. Yêu cầu VCPKG_ROOT trỏ tới vcpkg.
param([string]$Preset = "x64-release")

if (-not $env:VCPKG_ROOT) {
    Write-Warning "VCPKG_ROOT chưa được set. Dependency (krabsetw, sqlite3...) có thể không tìm thấy."
}

cmake --preset $Preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --build --preset $Preset
