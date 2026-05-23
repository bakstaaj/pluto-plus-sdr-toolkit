@echo off
setlocal

REM Pluto+ SDR Windows launcher
REM Place this .cmd file in your pluto_native_test project folder.
REM It expects:
REM   build\pluto_scan_session.exe
REM   configs\*.conf
REM
REM MSYS2 UCRT64 runtime DLLs are added to PATH here.

set "PROJECT_DIR=%~dp0"
cd /d "%PROJECT_DIR%"

set "MSYS2_ROOT=C:\msys64"
set "PATH=%MSYS2_ROOT%\ucrt64\bin;%MSYS2_ROOT%\usr\bin;%PATH%"

if not exist "build\pluto_scan_session.exe" (
    echo ERROR: build\pluto_scan_session.exe was not found.
    echo.
    echo Run this first from MSYS2 UCRT64:
    echo   cd ~/sdrdev/pluto_native_test
    echo   cmake --build build
    echo.
    pause
    exit /b 1
)


REM Generic profile runner.
REM Usage:
REM   run_profile.cmd configs\2m.conf
REM   run_profile.cmd configs\airband.conf --cycles 30
REM   run_profile.cmd configs\2m.conf --out-prefix 2m_evening

if "%~1"=="" (
    echo Usage:
    echo   run_profile.cmd configs\2m.conf [extra pluto_scan_session options]
    echo.
    echo Examples:
    echo   run_profile.cmd configs\2m.conf
    echo   run_profile.cmd configs\airband.conf --cycles 30
    echo   run_profile.cmd configs\2m.conf --gain-db 40 --out-prefix 2m_gain40
    echo.
    pause
    exit /b 1
)

set "CONFIG_FILE=%~1"
shift /1

if not exist "%CONFIG_FILE%" (
    echo ERROR: Config file not found: %CONFIG_FILE%
    pause
    exit /b 1
)

echo Running Pluto+ scan session with config:
echo   %CONFIG_FILE%
echo.

build\pluto_scan_session.exe --config "%CONFIG_FILE%" %*

if errorlevel 1 (
    echo.
    echo ERROR: scan session failed.
    pause
    exit /b 1
)

echo.
echo Scan session completed.
echo Check the generated *_report.html file in this folder.
echo.
pause
