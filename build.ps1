#requires -Version 5
<#
  Convenience build script for Copper Steam Idler.
  Configures with the MinGW/Qt preset, builds both executables and deploys the
  Qt runtime into the output folder so it can be launched directly.
#>
param(
    [string]$Preset = "mingw"
)

$ErrorActionPreference = "Stop"

$qtBin = "C:\Qt\6.11.2\mingw_64\bin"
$mingw = "C:\Qt\Tools\mingw1310_64\bin"
$ninja = "C:\Qt\Tools\Ninja"

$env:Path = "$qtBin;$mingw;$ninja;$env:Path"

Write-Host "==> Configure ($Preset)" -ForegroundColor Cyan
cmake --preset $Preset

Write-Host "==> Build" -ForegroundColor Cyan
cmake --build --preset $Preset

Write-Host "==> Deploy Qt runtime" -ForegroundColor Cyan
cmake --build --preset $Preset --target deploy

$out = Join-Path $PSScriptRoot "build\$Preset"
Write-Host "==> Done. Binaries in: $out" -ForegroundColor Green
