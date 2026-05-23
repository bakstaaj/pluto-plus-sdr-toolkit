@echo off
setlocal

REM This launcher lives in launchers\.

set "PROJECT_DIR=%~dp0.."
cd /d "%PROJECT_DIR%"

set "MSYS2_ROOT=C:\msys64"
set "PATH=%MSYS2_ROOT%\ucrt64\bin;%MSYS2_ROOT%\usr\bin;%PATH%"

if not exist "build\native\pluto_scan_session.exe" (
    echo ERROR: build\native\pluto_scan_session.exe was not found.
    echo Build from MSYS2 UCRT64:
    echo   ./tools/build_native_ucrt64.sh
    pause
    exit /b 1
)

build\native\pluto_scan_session.exe --config configs\noaa.conf %*

if errorlevel 1 (
    echo ERROR: scan session failed.
    pause
    exit /b 1
)

if exist "noaa_report.html" start "" "noaa_report.html"

pause
