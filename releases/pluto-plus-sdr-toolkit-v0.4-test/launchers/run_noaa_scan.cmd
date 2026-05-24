@echo off
setlocal

set "RELEASE_ROOT=%~dp0.."
set "BIN_DIR=%RELEASE_ROOT%\bin\native"
set "SESSION_DIR=%RELEASE_ROOT%\sessions"

mkdir "%SESSION_DIR%" 2>nul
cd /d "%SESSION_DIR%"

set "PATH=%BIN_DIR%;%PATH%"

if not exist "%BIN_DIR%\pluto_scan_session.exe" (
    echo ERROR: pluto_scan_session.exe not found:
    echo   %BIN_DIR%\pluto_scan_session.exe
    pause
    exit /b 1
)

if not exist "%RELEASE_ROOT%\configs\noaa.conf" (
    echo ERROR: config not found:
    echo   %RELEASE_ROOT%\configs\noaa.conf
    pause
    exit /b 1
)

echo Running Pluto+ NOAA scan session...
echo.

"%BIN_DIR%\pluto_scan_session.exe" --config "%RELEASE_ROOT%\configs\noaa.conf" %*

if errorlevel 1 (
    echo.
    echo ERROR: NOAA scan session failed.
    pause
    exit /b 1
)

if exist "%SESSION_DIR%\noaa_report.html" (
    start "" "%SESSION_DIR%\noaa_report.html"
)

echo.
echo Done. Output folder:
echo   %SESSION_DIR%
pause
