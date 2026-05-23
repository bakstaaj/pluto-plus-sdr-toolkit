#!/usr/bin/env bash
set -Eeuo pipefail

# create_release_launchers.sh
#
# Standalone repair tool.
#
# Usage from repo root:
#
#   ./tools/create_release_launchers.sh releases/pluto-plus-sdr-toolkit-v0.3-release-test
#
# Or with no argument, it uses the newest releases/pluto-plus-sdr-toolkit-* folder:
#
#   ./tools/create_release_launchers.sh
#
# This script does not build anything. It only writes launchers into:
#
#   <release-folder>/launchers/

ROOT="$(pwd)"
RELEASE_DIR="${1:-}"

if [ -z "${RELEASE_DIR}" ]; then
    if [ ! -d "${ROOT}/releases" ]; then
        echo "ERROR: releases folder not found."
        exit 1
    fi

    RELEASE_DIR="$(find "${ROOT}/releases" -maxdepth 1 -type d -name 'pluto-plus-sdr-toolkit-*' | sort | tail -n 1 || true)"
fi

if [ -z "${RELEASE_DIR}" ]; then
    echo "ERROR: Could not find a release folder."
    exit 1
fi

if [ ! -d "${RELEASE_DIR}" ]; then
    echo "ERROR: Release folder does not exist:"
    echo "  ${RELEASE_DIR}"
    exit 1
fi

LAUNCHER_DIR="${RELEASE_DIR}/launchers"

mkdir -p "${LAUNCHER_DIR}"

echo "Writing launchers into:"
echo "  ${LAUNCHER_DIR}"
echo

write_file() {
    local file="$1"
    shift

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

echo Running Pluto+ ${label} scan session...
echo.

"%BIN_DIR%\\pluto_scan_session.exe" --config "%RELEASE_ROOT%\\configs\\${config}" %*

if errorlevel 1 (
    echo.
    echo ERROR: ${label} scan session failed.
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
    echo   run_profile.cmd configs\airband.conf --out-prefix airband_evening
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

write_scan_launcher "run_fm_scan.cmd" "fm.conf" "fm" "FM broadcast"
write_scan_launcher "run_2m_scan.cmd" "2m.conf" "2m" "2 meter"
write_scan_launcher "run_airband_scan.cmd" "airband.conf" "airband" "airband"
write_scan_launcher "run_noaa_scan.cmd" "noaa.conf" "noaa" "NOAA"
write_scan_launcher "run_70cm_scan.cmd" "70cm.conf" "70cm" "70 cm"

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
set "GUI_EXE=%RELEASE_ROOT%\gui\PlutoSessionGui\PlutoGuiStarter.exe"

if exist "%GUI_EXE%" (
    start "" "%GUI_EXE%"
    exit /b 0
)

echo Session GUI executable was not found.
echo Expected:
echo   %GUI_EXE%
echo.
echo If the GUI was not published, run the source GUI with dotnet run.
pause
EOF

write_file "start_live_spectrum_gui.cmd" <<'EOF'
@echo off
setlocal

set "RELEASE_ROOT=%~dp0.."
set "GUI_EXE=%RELEASE_ROOT%\gui\PlutoLiveSpectrumGui\PlutoLiveSpectrumGui.exe"

if exist "%GUI_EXE%" (
    start "" "%GUI_EXE%"
    exit /b 0
)

echo Live Spectrum GUI executable was not found.
echo Expected:
echo   %GUI_EXE%
echo.
echo If the GUI was not published, run the source GUI with dotnet run.
pause
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
echo "Verifying:"
count="$(find "${LAUNCHER_DIR}" -maxdepth 1 -type f -name '*.cmd' | wc -l | tr -d ' ')"
find "${LAUNCHER_DIR}" -maxdepth 1 -type f -name '*.cmd' -printf "  %f\n" | sort

echo
echo "Launcher count: ${count}"

if [ "${count}" -lt 10 ]; then
    echo "ERROR: Expected 10 launchers, found ${count}."
    exit 1
fi

echo
echo "Release launchers repaired successfully."
