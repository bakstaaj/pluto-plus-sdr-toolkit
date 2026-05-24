@echo off
setlocal

REM Repo-level broadcast FM WBFM audio recorder launcher.

set "PROJECT_DIR=%~dp0.."
cd /d "%PROJECT_DIR%"

set "MSYS2_ROOT=C:\msys64"
set "PATH=%MSYS2_ROOT%\ucrt64\bin;%MSYS2_ROOT%\usr\bin;%PATH%"

set "AUDIO_EXE=%PROJECT_DIR%\build\native\pluto_audio_monitor.exe"
set "SESSION_DIR=%PROJECT_DIR%\sessions"

if not exist "%AUDIO_EXE%" (
    echo ERROR: pluto_audio_monitor.exe was not found.
    echo Expected:
    echo   %AUDIO_EXE%
    echo.
    echo Build first:
    echo   ./tools/build_native_ucrt64.sh
    pause
    exit /b 1
)

mkdir "%SESSION_DIR%" 2>nul
cd /d "%SESSION_DIR%"

echo Recording broadcast FM WBFM audio for 30 seconds...
echo Default preset:
echo   fm-100, 100.000 MHz
echo Output WAV:
echo   %SESSION_DIR%\fm100.wav
echo Output CSV:
echo   %SESSION_DIR%\audio_log.csv
echo.

"%AUDIO_EXE%" --preset fm-100 --seconds 30 --squelch-off --wav fm100.wav --csv audio_log.csv %*

if errorlevel 1 (
    echo.
    echo ERROR: FM WBFM audio recording failed.
    pause
    exit /b 1
)

echo.
echo Done.
echo WAV file:
echo   %SESSION_DIR%\fm100.wav
echo CSV log:
echo   %SESSION_DIR%\audio_log.csv
pause
