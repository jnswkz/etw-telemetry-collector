# Đóng gói ETW Telemetry Collector thành ZIP portable cài được trên máy sạch.
# Build release -> chạy unit test -> gom exe + config + script cài -> ZIP + SHA256.
#
# Dùng:  .\scripts\package.ps1              (build + test + đóng gói)
#        .\scripts\package.ps1 -SkipBuild   (dùng binary đã build sẵn)
# Kết quả: dist\etwcollector-<version>-win-x64.zip (+ .sha256)
param([switch]$SkipBuild)

$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$bin = Join-Path $root "build\x64-release\bin"

# 1. Phiên bản lấy từ project(VERSION ...) trong CMakeLists.txt.
$cm = Get-Content (Join-Path $root "CMakeLists.txt") -Raw
if ($cm -notmatch "VERSION\s+(\d+\.\d+\.\d+)") { throw "Không đọc được VERSION trong CMakeLists.txt" }
$version = $Matches[1]
$name = "etwcollector-$version-win-x64"
Write-Host "Đóng gói $name"

# 2. Build + test.
if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot "build.ps1") x64-release
}
$exe = Join-Path $bin "etwcollector.exe"
if (-not (Test-Path $exe)) { throw "Thiếu $exe — hãy build trước." }
& (Join-Path $bin "etwc_tests.exe") --reporter compact
if ($LASTEXITCODE -ne 0) { throw "Unit test thất bại ($LASTEXITCODE) — không đóng gói." }

# Bản static không được kèm DLL nào (sqlite3.dll xuất hiện = đang build triplet động).
$dlls = Get-ChildItem $bin -Filter *.dll -ErrorAction SilentlyContinue
if ($dlls) { throw "Thư mục bin có DLL ($($dlls.Name -join ', ')) — cần triplet x64-windows-static." }

# 3. Gom file. Script .ps1 ghi UTF-8 CÓ BOM để Windows PowerShell 5.1 đọc đúng tiếng Việt.
$dist = Join-Path $root "dist"
$stage = Join-Path $dist $name
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force -Path (Join-Path $stage "config") | Out-Null

Copy-Item $exe $stage
Copy-Item (Join-Path $root "config\collector.json") (Join-Path $stage "config")
$utf8Bom = New-Object System.Text.UTF8Encoding($true)
foreach ($f in @("install-service.ps1", "uninstall-service.ps1")) {
    $text = [IO.File]::ReadAllText((Join-Path $root "tools\$f"))
    [IO.File]::WriteAllText((Join-Path $stage $f), $text, $utf8Bom)
}
# Wrapper .cmd: bỏ qua ExecutionPolicy mặc định (Restricted) trên máy sạch.
foreach ($f in @("install", "uninstall")) {
    $cmd = "@echo off`r`npowershell -NoProfile -ExecutionPolicy Bypass -File `"%~dp0$f-service.ps1`" %*`r`npause`r`n"
    [IO.File]::WriteAllText((Join-Path $stage "$f.cmd"), $cmd, (New-Object System.Text.ASCIIEncoding))
}
Copy-Item (Join-Path $root "docs\INSTALL.md") $stage

# 4. ZIP + SHA256.
$zip = Join-Path $dist "$name.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip
$hash = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
"$hash  $name.zip" | Set-Content -Encoding ascii "$zip.sha256"

Write-Host "`nĐã tạo gói:" -ForegroundColor Green
Get-ChildItem $stage | Format-Table -AutoSize Name, Length
Write-Host "$zip"
Write-Host "SHA256: $hash"
