@echo off
setlocal

REM Pluto+ SDR Windows launcher
REM Place this .cmd file in your pluto_native_test project folder.
REM It expects:
REM   build\native\pluto_scan_session.exe
REM   configs\*.conf
REM
REM MSYS2 UCRT64 runtime DLLs are added to PATH here.

set "PROJECT_DIR=%~dp0"
cd /d "%PROJECT_DIR%"

set "MSYS2_ROOT=C:\msys64"
set "PATH=%MSYS2_ROOT%\ucrt64\bin;%MSYS2_ROOT%\usr\bin;%PATH%"

if not exist "build\native\pluto_scan_session.exe" (
    echo ERROR: build\native\pluto_scan_session.exe was not found.
    echo.
    echo Run this first from MSYS2 UCRT64:
    echo   cd ~/sdrdev/pluto_native_test
    echo   cmake --build build
    echo.
    pause
    exit /b 1
)


if not exist "configs\noaa.conf" (
    echo ERROR: configs\noaa.conf was not found.
    pause
    exit /b 1
)

echo Running Pluto+ NOAA scan session...
echo.

build\native\pluto_scan_session.exe --config configs\noaa.conf %*

if errorlevel 1 (
    echo.
    echo ERROR: NOAA scan session failed.
    pause
    exit /b 1
)

if exist "noaa_report.html" (
    echo.
    echo Opening noaa_report.html...
    start "" "noaa_report.html"
)

echo.
echo Done.
pause
