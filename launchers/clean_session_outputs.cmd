@echo off
setlocal

REM Deletes generated CSV and HTML session outputs from the project folder.
REM It does not delete source files, configs, or build outputs.

set "PROJECT_DIR=%~dp0"
cd /d "%PROJECT_DIR%"

echo This will delete generated scan/session files:
echo   *_raw.csv
echo   *_grouped.csv
echo   *_activity.csv
echo   *_summary.csv
echo   *_report.html
echo.
choice /M "Continue"

if errorlevel 2 (
    echo Cancelled.
    exit /b 0
)

del /q *_raw.csv 2>nul
del /q *_grouped.csv 2>nul
del /q *_activity.csv 2>nul
del /q *_summary.csv 2>nul
del /q *_report.html 2>nul

echo.
echo Generated session outputs cleaned.
pause
