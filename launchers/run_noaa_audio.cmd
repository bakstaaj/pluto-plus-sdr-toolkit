@echo off
setlocal

REM Repo-level NOAA NFM audio recorder launcher.

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

echo Recording NOAA NFM audio for 30 seconds...
echo Output:
echo   %SESSION_DIR%\noaa.wav
echo.

"%AUDIO_EXE%" --mode nfm --freq 162550000 --rate 960000 --audio-rate 48000 --seconds 30 --wav noaa.wav %*

if errorlevel 1 (
    echo.
    echo ERROR: NOAA audio recording failed.
    pause
    exit /b 1
)

echo.
echo Done.
echo WAV file:
echo   %SESSION_DIR%\noaa.wav
pause
