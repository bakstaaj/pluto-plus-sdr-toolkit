@echo off
setlocal

REM Repo-level audio report generator launcher.

set "PROJECT_DIR=%~dp0.."
cd /d "%PROJECT_DIR%"

set "REPORT_EXE=%PROJECT_DIR%\build\native\pluto_audio_report.exe"
set "SESSION_DIR=%PROJECT_DIR%\sessions"

if not exist "%REPORT_EXE%" (
    echo ERROR: pluto_audio_report.exe was not found.
    echo Expected:
    echo   %REPORT_EXE%
    echo.
    echo Build first:
    echo   ./tools/build_native_ucrt64.sh
    pause
    exit /b 1
)

mkdir "%SESSION_DIR%" 2>nul

if not exist "%SESSION_DIR%\audio_log.csv" (
    echo ERROR: audio_log.csv was not found:
    echo   %SESSION_DIR%\audio_log.csv
    echo.
    echo Run one of the audio launchers first:
    echo   launchers\run_audio_menu.cmd
    echo   launchers\run_noaa_audio.cmd
    echo   launchers\run_airband_audio.cmd
    echo   launchers\run_fm_audio.cmd
    pause
    exit /b 1
)

"%REPORT_EXE%" --in "%SESSION_DIR%\audio_log.csv" --out "%SESSION_DIR%\audio_report.html"

if errorlevel 1 (
    echo.
    echo ERROR: audio report generation failed.
    pause
    exit /b 1
)

start "" "%SESSION_DIR%\audio_report.html"
pause
