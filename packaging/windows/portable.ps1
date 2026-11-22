# =============================================================================
# TruthGram Windows portable packaging (no installer, single .zip)
# -----------------------------------------------------------------------------
# Builds a self-contained portable folder: TruthGram.exe + Qt runtime + theme.
# The portable folder writes its data to ./TruthGramData/ next to the exe
# instead of %APPDATA% — handled by the runtime via --portable flag.
# =============================================================================
[CmdletBinding()]
param(
    [string]$BuildType = 'Release',
    [string]$OutDir    = "$PSScriptRoot\..\dist"
)
$ErrorActionPreference = 'Stop'

& "$PSScriptRoot\build.ps1" -BuildType $BuildType -Portable
if ($LASTEXITCODE -ne 0) { throw "build.ps1 -Portable failed." }

# Emit a tiny launcher that forces --portable and a side-by-side data dir.
$repoRoot = Resolve-Path "$PSScriptRoot\..\.."
$version  = (Get-Content "$repoRoot\CMakeLists.txt" |
    Select-String 'VERSION (\d+\.\d+\.\d+)').Matches.Groups[1].Value

$staging = "$OutDir\staging"
$portableRoot = "$OutDir\TruthGram-$version-portable"
if (Test-Path $portableRoot) { Remove-Item $portableRoot -Recurse -Force }
Copy-Item $staging $portableRoot -Recurse

$bat = @"
@echo off
start "" "%~dp0TruthGram.exe" --portable --workdir "%~dp0TruthGramData"
"@
Set-Content -Path "$portableRoot\TruthGramPortable.bat" -Value $bat -Encoding ASCII

$zip = "$OutDir\TruthGram-$version-portable.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path "$portableRoot\*" -DestinationPath $zip
Write-Host "Portable zip: $zip"
