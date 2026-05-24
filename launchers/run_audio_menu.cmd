@echo off
setlocal EnableDelayedExpansion

REM Pluto+ SDR Audio Menu Launcher
REM
REM Important:
REM   This launcher does NOT store the full command line in one variable.
REM   It stores only the argument list, then runs:
REM
REM     "%AUDIO_EXE%" !ARGS!
REM
REM   This avoids the Windows cmd.exe bug where the entire command plus arguments
REM   can be treated as one executable name.

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
    echo Build first from MSYS2 UCRT64:
    echo   cd ~/sdrdev/pluto_native_test
    echo   ./tools/build_native_ucrt64.sh
    pause
    exit /b 1
)

mkdir "%SESSION_DIR%" 2>nul
cd /d "%SESSION_DIR%"

:menu
cls
echo Pluto+ SDR Audio Menu
echo =====================
echo.
echo Output folder:
echo   %SESSION_DIR%
echo.
echo  1. NOAA NFM, 162.550 MHz, 30 seconds
echo  2. NOAA NFM, choose NOAA preset
echo  3. Airband AM, 125.000 MHz, 60 seconds
echo  4. Airband AM, long capture, 180 seconds
echo  5. Broadcast FM WBFM, 100.000 MHz, 30 seconds
echo  6. Broadcast FM WBFM, choose FM preset
echo  7. Custom frequency and mode
echo  8. Open sessions folder
echo  9. Exit
echo.
set /p CHOICE="Select option: "

if "%CHOICE%"=="1" goto noaa_default
if "%CHOICE%"=="2" goto noaa_choose
if "%CHOICE%"=="3" goto airband_default
if "%CHOICE%"=="4" goto airband_long
if "%CHOICE%"=="5" goto fm_default
if "%CHOICE%"=="6" goto fm_choose
if "%CHOICE%"=="7" goto custom
if "%CHOICE%"=="8" goto open_folder
if "%CHOICE%"=="9" goto done

echo.
echo Invalid choice.
pause
goto menu

:noaa_default
set "ARGS=--mode nfm --preset noaa7 --rate 960000 --audio-rate 48000 --seconds 30 --squelch-db -65 --wav noaa.wav --csv audio_log.csv"
goto run_command

:noaa_choose
cls
echo NOAA presets:
echo   noaa1 = 162.400 MHz
echo   noaa2 = 162.425 MHz
echo   noaa3 = 162.450 MHz
echo   noaa4 = 162.475 MHz
echo   noaa5 = 162.500 MHz
echo   noaa6 = 162.525 MHz
echo   noaa7 = 162.550 MHz
echo.
set /p PRESET="Enter NOAA preset [noaa7]: "
if "%PRESET%"=="" set "PRESET=noaa7"
set "ARGS=--mode nfm --preset %PRESET% --rate 960000 --audio-rate 48000 --seconds 30 --squelch-db -65 --wav %PRESET%.wav --csv audio_log.csv"
goto run_command

:airband_default
set "ARGS=--mode am --preset airband-125 --rate 960000 --audio-rate 48000 --seconds 60 --squelch-db -65 --wav airband_am.wav --csv audio_log.csv"
goto run_command

:airband_long
set "ARGS=--mode am --preset airband-125 --rate 960000 --audio-rate 48000 --seconds 180 --squelch-db -70 --wav airband_am_180s.wav --csv audio_log.csv"
goto run_command

:fm_default
set "ARGS=--preset fm-100 --seconds 30 --squelch-off --wav fm100.wav --csv audio_log.csv"
goto run_command

:fm_choose
cls
echo FM presets:
echo   fm-88
echo   fm-90
echo   fm-94
echo   fm-98
echo   fm-100
echo   fm-102
echo   fm-104
echo   fm-106
echo.
set /p PRESET="Enter FM preset [fm-100]: "
if "%PRESET%"=="" set "PRESET=fm-100"
set "ARGS=--preset %PRESET% --seconds 30 --squelch-off --wav %PRESET%.wav --csv audio_log.csv"
goto run_command

:custom
cls
echo Custom audio capture
echo.
echo Modes:
echo   nfm
echo   am
echo   wbfm
echo.
set /p MODE="Mode [nfm/am/wbfm]: "
if "%MODE%"=="" set "MODE=nfm"

set /p FREQ="Frequency Hz, example 162550000: "
if "%FREQ%"=="" (
    echo ERROR: frequency is required.
    pause
    goto menu
)

set /p SECONDS="Seconds [30]: "
if "%SECONDS%"=="" set "SECONDS=30"

set /p WAV="WAV filename [custom_audio.wav]: "
if "%WAV%"=="" set "WAV=custom_audio.wav"

if /I "%MODE%"=="wbfm" (
    set "ARGS=--mode wbfm --freq %FREQ% --rate 2400000 --bw 1800000 --audio-lowpass-hz 15000 --seconds %SECONDS% --squelch-off --wav %WAV% --csv audio_log.csv"
) else if /I "%MODE%"=="am" (
    set "ARGS=--mode am --freq %FREQ% --rate 960000 --bw 200000 --seconds %SECONDS% --squelch-db -65 --wav %WAV% --csv audio_log.csv"
) else (
    set "ARGS=--mode nfm --freq %FREQ% --rate 960000 --bw 200000 --seconds %SECONDS% --squelch-db -65 --wav %WAV% --csv audio_log.csv"
)
goto run_command

:open_folder
start "" "%SESSION_DIR%"
goto menu

:run_command
cls
echo Running:
echo   "%AUDIO_EXE%" !ARGS!
echo.
"%AUDIO_EXE%" !ARGS!

if errorlevel 1 (
    echo.
    echo ERROR: audio command failed.
    pause
    goto menu
)

echo.
echo Done. Output folder:
echo   %SESSION_DIR%
echo.
pause
goto menu

:done
exit /b 0
