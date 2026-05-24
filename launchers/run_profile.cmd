@echo off
setlocal

REM Repo-level generic Pluto+ scan profile launcher.
REM This file lives in:
REM   launchers\run_profile.cmd
REM
REM Usage from repo root or launchers folder:
REM   launchers\run_profile.cmd configs\fm.conf --dry-run
REM   launchers\run_profile.cmd configs\2m.conf --cycles 50

set "PROJECT_DIR=%~dp0.."
cd /d "%PROJECT_DIR%"

set "MSYS2_ROOT=C:\msys64"
set "PATH=%MSYS2_ROOT%\ucrt64\bin;%MSYS2_ROOT%\usr\bin;%PATH%"

set "SCAN_SESSION_EXE=%PROJECT_DIR%\build\native\pluto_scan_session.exe"

if "%~1"=="" (
    echo Usage:
    echo   run_profile.cmd configs\2m.conf [extra pluto_scan_session options]
    echo.
    echo Examples:
    echo   run_profile.cmd configs\fm.conf --dry-run
    echo   run_profile.cmd configs\2m.conf --cycles 50
    echo   run_profile.cmd configs\airband.conf --out-prefix airband_evening
    echo.
    pause
    exit /b 1
)

set "CONFIG_FILE=%~1"
shift /1

if not exist "%SCAN_SESSION_EXE%" (
    echo ERROR: pluto_scan_session.exe was not found.
    echo Expected:
    echo   %SCAN_SESSION_EXE%
    echo.
    echo Build first from MSYS2 UCRT64:
    echo   cd ~/sdrdev/pluto_native_test
    echo   ./tools/build_native_ucrt64.sh
    echo.
    pause
    exit /b 1
)

if not exist "%PROJECT_DIR%\%CONFIG_FILE%" (
    echo ERROR: Config file was not found.
    echo Expected:
    echo   %PROJECT_DIR%\%CONFIG_FILE%
    echo.
    pause
    exit /b 1
)

echo Running Pluto+ scan session with config:
echo   %CONFIG_FILE%
echo.
echo Executable:
echo   %SCAN_SESSION_EXE%
echo.

"%SCAN_SESSION_EXE%" --config "%PROJECT_DIR%\%CONFIG_FILE%" %*

if errorlevel 1 (
    echo.
    echo ERROR: scan session failed.
    pause
    exit /b 1
)

echo.
echo Scan session complete.
pause
