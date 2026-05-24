#!/usr/bin/env bash
set -Eeuo pipefail

# create_release_launchers.sh
#
# Standalone release launcher generator.
#
# Includes:
#   scan launchers
#   audio launchers
#   report-aware audio menu launcher with fixed quoting
#   audio HTML report launcher
#   GUI launchers
#
# Permanent audio menu report workflow:
#   run_audio_menu.cmd can now generate/open audio_report.html directly.
#
# Usage:
#   ./tools/create_release_launchers.sh releases/pluto-plus-sdr-toolkit-v1.4-audio-report-menu

RELEASE_DIR="${1:-}"

if [ -z "${RELEASE_DIR}" ] || [ ! -d "${RELEASE_DIR}" ]; then
    echo "ERROR: release folder argument is required and must exist."
    echo "Usage:"
    echo "  ./tools/create_release_launchers.sh releases/pluto-plus-sdr-toolkit-v1.4-audio-report-menu"
    exit 1
fi

LAUNCHER_DIR="${RELEASE_DIR}/launchers"
mkdir -p "${LAUNCHER_DIR}"

echo "Writing launchers into:"
echo "  ${LAUNCHER_DIR}"
echo

write_file() {
    local file="$1"
    cat > "${LAUNCHER_DIR}/${file}"
    chmod 0644 "${LAUNCHER_DIR}/${file}" 2>/dev/null || true
    echo "  wrote ${file}"
}

write_scan_launcher() {
    local file="$1"
    local config="$2"
    local prefix="$3"
    local label="$4"

    write_file "${file}" <<EOF
@echo off
setlocal

set "RELEASE_ROOT=%~dp0.."
set "BIN_DIR=%RELEASE_ROOT%\\bin\\native"
set "SESSION_DIR=%RELEASE_ROOT%\\sessions"

mkdir "%SESSION_DIR%" 2>nul
cd /d "%SESSION_DIR%"

set "PATH=%BIN_DIR%;%PATH%"

if not exist "%BIN_DIR%\\pluto_scan_session.exe" (
    echo ERROR: pluto_scan_session.exe not found:
    echo   %BIN_DIR%\\pluto_scan_session.exe
    pause
    exit /b 1
)

if not exist "%RELEASE_ROOT%\\configs\\${config}" (
    echo ERROR: config not found:
    echo   %RELEASE_ROOT%\\configs\\${config}
    pause
    exit /b 1
)

echo Running Pluto+ ${label}...
echo.

"%BIN_DIR%\\pluto_scan_session.exe" --config "%RELEASE_ROOT%\\configs\\${config}" %*

if errorlevel 1 (
    echo.
    echo ERROR: ${label} failed.
    pause
    exit /b 1
)

if exist "%SESSION_DIR%\\${prefix}_report.html" (
    start "" "%SESSION_DIR%\\${prefix}_report.html"
)

echo.
echo Done. Output folder:
echo   %SESSION_DIR%
pause
EOF
}

write_scan_launcher "run_fm_scan.cmd" "fm.conf" "fm" "FM broadcast scan"
write_scan_launcher "run_2m_scan.cmd" "2m.conf" "2m" "2 meter scan"
write_scan_launcher "run_airband_scan.cmd" "airband.conf" "airband" "airband scan"
write_scan_launcher "run_noaa_scan.cmd" "noaa.conf" "noaa" "NOAA scan"
write_scan_launcher "run_70cm_scan.cmd" "70cm.conf" "70 cm" "70 cm scan"

write_file "run_profile.cmd" <<'EOF'
@echo off
setlocal

set "RELEASE_ROOT=%~dp0.."
set "BIN_DIR=%RELEASE_ROOT%\bin\native"
set "SESSION_DIR=%RELEASE_ROOT%\sessions"

if "%~1"=="" (
    echo Usage:
    echo   run_profile.cmd configs\2m.conf [extra options]
    echo.
    echo Examples:
    echo   run_profile.cmd configs\fm.conf
    echo   run_profile.cmd configs\2m.conf --cycles 50
    echo.
    pause
    exit /b 1
)

set "CONFIG_FILE=%~1"
shift /1

if not exist "%RELEASE_ROOT%\%CONFIG_FILE%" (
    echo ERROR: Config file not found:
    echo   %RELEASE_ROOT%\%CONFIG_FILE%
    pause
    exit /b 1
)

if not exist "%BIN_DIR%\pluto_scan_session.exe" (
    echo ERROR: pluto_scan_session.exe not found:
    echo   %BIN_DIR%\pluto_scan_session.exe
    pause
    exit /b 1
)

mkdir "%SESSION_DIR%" 2>nul
cd /d "%SESSION_DIR%"
set "PATH=%BIN_DIR%;%PATH%"

"%BIN_DIR%\pluto_scan_session.exe" --config "%RELEASE_ROOT%\%CONFIG_FILE%" %*

if errorlevel 1 (
    echo.
    echo ERROR: scan session failed.
    pause
    exit /b 1
)

echo.
echo Scan session complete. Output folder:
echo   %SESSION_DIR%
pause
EOF

write_file "run_noaa_audio.cmd" <<'EOF'
@echo off
setlocal

set "RELEASE_ROOT=%~dp0.."
set "BIN_DIR=%RELEASE_ROOT%\bin\native"
set "SESSION_DIR=%RELEASE_ROOT%\sessions"

mkdir "%SESSION_DIR%" 2>nul
cd /d "%SESSION_DIR%"
set "PATH=%BIN_DIR%;%PATH%"

if not exist "%BIN_DIR%\pluto_audio_monitor.exe" (
    echo ERROR: pluto_audio_monitor.exe not found:
    echo   %BIN_DIR%\pluto_audio_monitor.exe
    pause
    exit /b 1
)

echo Recording NOAA NFM audio for 30 seconds...
echo Output WAV:
echo   %SESSION_DIR%\noaa.wav
echo Output CSV:
echo   %SESSION_DIR%\audio_log.csv
echo.

"%BIN_DIR%\pluto_audio_monitor.exe" --mode nfm --preset noaa7 --rate 960000 --audio-rate 48000 --seconds 30 --squelch-db -65 --wav noaa.wav --csv audio_log.csv %*

if errorlevel 1 (
    echo.
    echo ERROR: NOAA audio recording failed.
    pause
    exit /b 1
)

echo.
echo Done.
start "" "%SESSION_DIR%"
pause
EOF

write_file "run_airband_audio.cmd" <<'EOF'
@echo off
setlocal

set "RELEASE_ROOT=%~dp0.."
set "BIN_DIR=%RELEASE_ROOT%\bin\native"
set "SESSION_DIR=%RELEASE_ROOT%\sessions"

mkdir "%SESSION_DIR%" 2>nul
cd /d "%SESSION_DIR%"
set "PATH=%BIN_DIR%;%PATH%"

if not exist "%BIN_DIR%\pluto_audio_monitor.exe" (
    echo ERROR: pluto_audio_monitor.exe not found:
    echo   %BIN_DIR%\pluto_audio_monitor.exe
    pause
    exit /b 1
)

echo Recording airband AM audio for 60 seconds...
echo Output WAV:
echo   %SESSION_DIR%\airband_am.wav
echo Output CSV:
echo   %SESSION_DIR%\audio_log.csv
echo.

"%BIN_DIR%\pluto_audio_monitor.exe" --mode am --preset airband-125 --rate 960000 --audio-rate 48000 --seconds 60 --squelch-db -65 --wav airband_am.wav --csv audio_log.csv %*

if errorlevel 1 (
    echo.
    echo ERROR: airband AM audio recording failed.
    pause
    exit /b 1
)

echo.
echo Done.
start "" "%SESSION_DIR%"
pause
EOF

write_file "run_fm_audio.cmd" <<'EOF'
@echo off
setlocal

set "RELEASE_ROOT=%~dp0.."
set "BIN_DIR=%RELEASE_ROOT%\bin\native"
set "SESSION_DIR=%RELEASE_ROOT%\sessions"

mkdir "%SESSION_DIR%" 2>nul
cd /d "%SESSION_DIR%"
set "PATH=%BIN_DIR%;%PATH%"

if not exist "%BIN_DIR%\pluto_audio_monitor.exe" (
    echo ERROR: pluto_audio_monitor.exe not found:
    echo   %BIN_DIR%\pluto_audio_monitor.exe
    pause
    exit /b 1
)

echo Recording broadcast FM WBFM audio for 30 seconds...
echo Output WAV:
echo   %SESSION_DIR%\fm100.wav
echo Output CSV:
echo   %SESSION_DIR%\audio_log.csv
echo.

"%BIN_DIR%\pluto_audio_monitor.exe" --preset fm-100 --seconds 30 --squelch-off --wav fm100.wav --csv audio_log.csv %*

if errorlevel 1 (
    echo.
    echo ERROR: broadcast FM WBFM audio recording failed.
    pause
    exit /b 1
)

echo.
echo Done.
start "" "%SESSION_DIR%"
pause
EOF

write_file "run_audio_menu.cmd" <<'EOF'
@echo off
setlocal EnableDelayedExpansion

REM Pluto+ SDR Audio Menu Launcher
REM Release version.
REM
REM Features:
REM   - Records NOAA NFM, airband AM, and broadcast FM WBFM audio
REM   - Uses safe quoted execution:
REM       "%AUDIO_EXE%" !ARGS!
REM   - Can generate sessions\audio_report.html using pluto_audio_report.exe
REM   - Offers to update the report after each successful recording

set "RELEASE_ROOT=%~dp0.."
set "BIN_DIR=%RELEASE_ROOT%\bin\native"
set "SESSION_DIR=%RELEASE_ROOT%\sessions"
set "AUDIO_EXE=%BIN_DIR%\pluto_audio_monitor.exe"
set "REPORT_EXE=%BIN_DIR%\pluto_audio_report.exe"

set "PATH=%BIN_DIR%;%PATH%"

if not exist "%AUDIO_EXE%" (
    echo ERROR: pluto_audio_monitor.exe not found:
    echo   %AUDIO_EXE%
    echo.
    echo The release package may be incomplete.
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
echo  1.  NOAA NFM, 162.550 MHz, 30 seconds
echo  2.  NOAA NFM, choose NOAA preset
echo  3.  Airband AM, 125.000 MHz, 60 seconds
echo  4.  Airband AM, long capture, 180 seconds
echo  5.  Broadcast FM WBFM, 100.000 MHz, 30 seconds
echo  6.  Broadcast FM WBFM, choose FM preset
echo  7.  Custom frequency and mode
echo  8.  Generate / open audio HTML report
echo  9.  Open sessions folder
echo  10. Exit
echo.
set /p CHOICE="Select option: "

if "%CHOICE%"=="1" goto noaa_default
if "%CHOICE%"=="2" goto noaa_choose
if "%CHOICE%"=="3" goto airband_default
if "%CHOICE%"=="4" goto airband_long
if "%CHOICE%"=="5" goto fm_default
if "%CHOICE%"=="6" goto fm_choose
if "%CHOICE%"=="7" goto custom
if "%CHOICE%"=="8" goto make_report
if "%CHOICE%"=="9" goto open_folder
if "%CHOICE%"=="10" goto done

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
echo   nfm   NOAA / 2m FM voice
echo   am    airband AM
echo   wbfm  FM broadcast
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
echo Recording complete.
echo Output folder:
echo   %SESSION_DIR%
echo.

if exist "%REPORT_EXE%" if exist "%SESSION_DIR%\audio_log.csv" (
    set /p GENREPORT="Generate/update audio_report.html now? [Y/n]: "
    if /I not "!GENREPORT!"=="n" goto make_report
)

pause
goto menu

:make_report
cls
echo Generating audio report...
echo.

if not exist "%REPORT_EXE%" (
    echo ERROR: pluto_audio_report.exe was not found.
    echo Expected:
    echo   %REPORT_EXE%
    echo.
    echo The release package may be incomplete.
    pause
    goto menu
)

if not exist "%SESSION_DIR%\audio_log.csv" (
    echo ERROR: audio_log.csv was not found:
    echo   %SESSION_DIR%\audio_log.csv
    echo.
    echo Record audio first.
    pause
    goto menu
)

"%REPORT_EXE%" --in "%SESSION_DIR%\audio_log.csv" --out "%SESSION_DIR%\audio_report.html"

if errorlevel 1 (
    echo.
    echo ERROR: audio report generation failed.
    pause
    goto menu
)

echo.
echo Audio report written:
echo   %SESSION_DIR%\audio_report.html
echo.

start "" "%SESSION_DIR%\audio_report.html"
pause
goto menu

:done
exit /b 0
EOF

write_file "make_audio_report.cmd" <<'EOF'
@echo off
setlocal

set "RELEASE_ROOT=%~dp0.."
set "BIN_DIR=%RELEASE_ROOT%\bin\native"
set "SESSION_DIR=%RELEASE_ROOT%\sessions"
set "REPORT_EXE=%BIN_DIR%\pluto_audio_report.exe"

mkdir "%SESSION_DIR%" 2>nul

if not exist "%REPORT_EXE%" (
    echo ERROR: pluto_audio_report.exe was not found.
    echo Expected:
    echo   %REPORT_EXE%
    pause
    exit /b 1
)

if not exist "%SESSION_DIR%\audio_log.csv" (
    echo ERROR: audio_log.csv was not found:
    echo   %SESSION_DIR%\audio_log.csv
    echo.
    echo Run an audio recording first:
    echo   run_audio_menu.cmd
    echo   run_noaa_audio.cmd
    echo   run_airband_audio.cmd
    echo   run_fm_audio.cmd
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
EOF

write_file "open_sessions_folder.cmd" <<'EOF'
@echo off
setlocal
set "RELEASE_ROOT=%~dp0.."
mkdir "%RELEASE_ROOT%\sessions" 2>nul
start "" "%RELEASE_ROOT%\sessions"
EOF

write_file "start_session_gui.cmd" <<'EOF'
@echo off
setlocal

set "RELEASE_ROOT=%~dp0.."
set "GUI_DIR=%RELEASE_ROOT%\gui\PlutoSessionGui"

if not exist "%GUI_DIR%" (
    echo Session GUI folder was not found.
    echo Expected:
    echo   %GUI_DIR%
    pause
    exit /b 1
)

for %%F in ("%GUI_DIR%\*.exe") do (
    start "" "%%~fF"
    exit /b 0
)

echo Session GUI executable was not found in:
echo   %GUI_DIR%
echo.
dir "%GUI_DIR%"
pause
exit /b 1
EOF

write_file "start_live_spectrum_gui.cmd" <<'EOF'
@echo off
setlocal

set "RELEASE_ROOT=%~dp0.."
set "GUI_DIR=%RELEASE_ROOT%\gui\PlutoLiveSpectrumGui"

if not exist "%GUI_DIR%" (
    echo Live Spectrum GUI folder was not found.
    echo Expected:
    echo   %GUI_DIR%
    pause
    exit /b 1
)

for %%F in ("%GUI_DIR%\*.exe") do (
    start "" "%%~fF"
    exit /b 0
)

echo Live Spectrum GUI executable was not found in:
echo   %GUI_DIR%
echo.
dir "%GUI_DIR%"
pause
exit /b 1
EOF

write_file "start_live_spectrum_stream_fm.cmd" <<'EOF'
@echo off
setlocal

set "RELEASE_ROOT=%~dp0.."
set "BIN_DIR=%RELEASE_ROOT%\bin\native"

set "PATH=%BIN_DIR%;%PATH%"

if not exist "%BIN_DIR%\pluto_spectrum_stream.exe" (
    echo ERROR: pluto_spectrum_stream.exe not found:
    echo   %BIN_DIR%\pluto_spectrum_stream.exe
    pause
    exit /b 1
)

"%BIN_DIR%\pluto_spectrum_stream.exe" --freq 100000000 --rate 2000000 --fft 2048 --avg 2

pause
EOF

echo
echo "Verifying release launchers:"
count="$(find "${LAUNCHER_DIR}" -maxdepth 1 -type f -name '*.cmd' | wc -l | tr -d ' ')"
find "${LAUNCHER_DIR}" -maxdepth 1 -type f -name '*.cmd' -printf "  %f\n" | sort

echo
echo "Launcher count: ${count}"

if [ "${count}" -lt 15 ]; then
    echo "ERROR: Expected at least 15 launchers, found ${count}."
    exit 1
fi

for required in \
    run_audio_menu.cmd \
    run_noaa_audio.cmd \
    run_airband_audio.cmd \
    run_fm_audio.cmd \
    make_audio_report.cmd
do
    if [ ! -f "${LAUNCHER_DIR}/${required}" ]; then
        echo "ERROR: missing launcher ${required}"
        exit 1
    fi
done

if ! grep -q '"%AUDIO_EXE%" !ARGS!' "${LAUNCHER_DIR}/run_audio_menu.cmd"; then
    echo "ERROR: run_audio_menu.cmd does not contain fixed quoted execution."
    exit 1
fi

if ! grep -q "Generate/update audio_report.html now" "${LAUNCHER_DIR}/run_audio_menu.cmd"; then
    echo "ERROR: run_audio_menu.cmd does not contain report workflow prompt."
    exit 1
fi

if ! grep -q "pluto_audio_report.exe" "${LAUNCHER_DIR}/run_audio_menu.cmd"; then
    echo "ERROR: run_audio_menu.cmd does not reference pluto_audio_report.exe."
    exit 1
fi

echo
echo "Release launchers created successfully."
