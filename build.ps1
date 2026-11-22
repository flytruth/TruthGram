# =============================================================================
# TruthGram Windows build driver (MSVC + Ninja)
# -----------------------------------------------------------------------------
# Usage:
#   .\build.ps1 -BuildType Release -Portable      # -> dist\TruthGram-x.y.z-portable.zip
#   .\build.ps1 -BuildType Release -Installer      # -> dist\TruthGram-x.y.z-setup.exe
#   .\build.ps1 -BuildType Debug   -Asan
# =============================================================================
[CmdletBinding()]
param(
    [ValidateSet('Debug','Release','RelWithDebInfo')]
    [string]$BuildType = 'Release',

    [switch]$Portable,
    [switch]$Installer,
    [switch]$Asan,
    [string]$TdesktopDir = "$PSScriptRoot\..\..\tdesktop",
    [string]$OutDir      = "$PSScriptRoot\..\dist"
)

$ErrorActionPreference = 'Stop'
$repoRoot = Resolve-Path "$PSScriptRoot\..\.."

# --- Locate Visual Studio via vswhere ---
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found. Install VS 2022." }
$vsPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild `
    -property installationPath
if (-not $vsPath) { throw "Visual Studio 2022 with MSBuild not found." }
Write-Host "VS path: $vsPath"

# Import the MSVC dev environment (sets INCLUDE, LIB, PATH for cl.exe/ninja).
$devShell = "$vsPath\Common7\Tools\Launch-VsDevShell.ps1"
& $devShell -Arch amd64 -HostArch amd64 | Out-Null

# --- Configure ---
cmake --preset windows-msvc-ninja `
    -DCMAKE_BUILD_TYPE=$BuildType `
    "-DTRUTHGRAM_TDESKTOP_DIR=$TdesktopDir" `
    "-DTRUTHGRAM_ENABLE_ASAN=$([int]$Asan.IsPresent)"

if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

# --- Build ---
cmake --build --preset windows-msvc-ninja
if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }

# --- Stage ---
$staging = "$OutDir\staging"
if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }
New-Item -ItemType Directory -Force -Path $staging | Out-Null

$binDir = "$repoRoot\build\windows-msvc-ninja\bin"
Copy-Item "$binDir\TruthGram.exe" $staging
# Qt windeployqt to bundle the runtime.
$qtBin = (Get-Command windeployqt -ErrorAction SilentlyContinue).Source
if ($qtBin) {
    & $qtBin --release --no-translations --no-opengl-sw `
        --multimedia --quick --concurrent "$staging\TruthGram.exe"
}
Copy-Item "$repoRoot\LICENSE" $staging
Copy-Item "$repoRoot\README.md" $staging
Copy-Item "$repoRoot\resources\themes\cyber-truth.tdesktop-theme" `
    "$staging\themes\"

# --- Produce artifact(s) ---
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$version = (Get-Content "$repoRoot\CMakeLists.txt" |
    Select-String 'VERSION (\d+\.\d+\.\d+)').Matches.Groups[1].Value

if ($Portable.IsPresent) {
    $zip = "$OutDir\TruthGram-$version-portable.zip"
    Compress-Archive -Path "$staging\*" -DestinationPath $zip -Force
    Write-Host "Portable: $zip"
}
if ($Installer.IsPresent) {
    $iscc = (Get-Command ISCC -ErrorAction SilentlyContinue).Source
    if (-not $iscc) { throw "Inno Setup (ISCC) not on PATH." }
    & $iscc /Q "/DTRUTHGRAM_VERSION=$version" `
        "/DTRUTHGRAM_STAGING=$staging" `
        "$PSScriptRoot\TruthGram.iss"
    Write-Host "Installer: $OutDir\TruthGram-$version-setup.exe"
}
if (-not ($Portable.IsPresent -or $Installer.IsPresent)) {
    Write-Host "Staging only: $staging"
}
