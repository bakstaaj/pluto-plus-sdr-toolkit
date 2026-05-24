@echo off
setlocal

REM Repo-level airband AM audio recorder launcher.

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

echo Recording airband AM audio for 30 seconds...
echo Default preset:
echo   airband-125, 125.000 MHz
echo Output WAV:
echo   %SESSION_DIR%\airband_am.wav
echo Output CSV:
echo   %SESSION_DIR%\audio_log.csv
echo.

"%AUDIO_EXE%" --mode am --preset airband-125 --rate 960000 --audio-rate 48000 --seconds 30 --squelch-db -65 --wav airband_am.wav --csv audio_log.csv %*

if errorlevel 1 (
    echo.
    echo ERROR: airband AM audio recording failed.
    pause
    exit /b 1
)

echo.
echo Done.
echo WAV file:
echo   %SESSION_DIR%\airband_am.wav
echo CSV log:
echo   %SESSION_DIR%\audio_log.csv
pause
