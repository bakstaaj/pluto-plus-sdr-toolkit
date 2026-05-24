@echo off
setlocal

REM Pluto+ FM broadcast scan launcher
REM Place this .cmd file in your pluto_native_test project folder.

set "PROJECT_DIR=%~dp0"
cd /d "%PROJECT_DIR%"

set "MSYS2_ROOT=C:\msys64"
set "PATH=%MSYS2_ROOT%\ucrt64\bin;%MSYS2_ROOT%\usr\bin;%PATH%"

if not exist "build\native\pluto_scan_session.exe" (
    echo ERROR: build\native\pluto_scan_session.exe was not found.
    pause
    exit /b 1
)

if not exist "configs\fm.conf" (
    echo ERROR: configs\fm.conf was not found.
    pause
    exit /b 1
)

echo Running Pluto+ FM broadcast scan session...
echo.

build\native\pluto_scan_session.exe --config configs\fm.conf %*

if errorlevel 1 (
    echo.
    echo ERROR: FM scan session failed.
    pause
    exit /b 1
)

if exist "fm_report.html" (
    echo.
    echo Opening fm_report.html...
    start "" "fm_report.html"
)

echo.
echo Done.
pause
