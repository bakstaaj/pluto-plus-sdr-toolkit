param(
    [string]$Version = (Get-Date -Format "yyyyMMdd-HHmmss")
)

# Simple PowerShell wrapper. The full package script is the MSYS2 bash script:
#   tools/package_windows_release.sh
#
# This wrapper calls it from PowerShell if bash is available.

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$bash = "C:\msys64\usr\bin\bash.exe"

if (!(Test-Path $bash)) {
    Write-Error "MSYS2 bash not found at $bash. Run tools/package_windows_release.sh from MSYS2 UCRT64 instead."
}

& $bash -lc "cd '$($root.Replace('\','/').Replace('C:','/c'))' && ./tools/package_windows_release.sh '$Version'"
