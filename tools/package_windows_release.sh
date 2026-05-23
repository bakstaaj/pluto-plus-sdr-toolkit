#!/usr/bin/env bash
set -euo pipefail

# package_windows_release.sh
#
# Run from the repository root inside MSYS2 UCRT64:
#
#   ./tools/package_windows_release.sh
#
# Optional:
#
#   ./tools/package_windows_release.sh v0.3-test
#
# Output:
#
#   releases/pluto-plus-sdr-toolkit-<version-or-timestamp>/
#   releases/pluto-plus-sdr-toolkit-<version-or-timestamp>.zip
#
# This script expects the reorganized repo layout:
#
#   native/src/
#   configs/
#   launchers/
#   gui/
#   docs/
#   build/native/

APP_NAME="pluto-plus-sdr-toolkit"
VERSION="${1:-$(date +%Y%m%d-%H%M%S)}"
RELEASE_NAME="${APP_NAME}-${VERSION}"

ROOT="$(pwd)"
BUILD_DIR="${ROOT}/build/native"
RELEASES_DIR="${ROOT}/releases"
RELEASE_DIR="${RELEASES_DIR}/${RELEASE_NAME}"

BIN_DIR="${RELEASE_DIR}/bin/native"
CONFIG_DIR="${RELEASE_DIR}/configs"
LAUNCHER_DIR="${RELEASE_DIR}/launchers"
DOCS_DIR="${RELEASE_DIR}/docs"
GUI_DIR="${RELEASE_DIR}/gui"
SESSIONS_DIR="${RELEASE_DIR}/sessions"

echo "Creating Windows release package"
echo "Root:        ${ROOT}"
echo "Release:     ${RELEASE_DIR}"
echo

mkdir -p "${RELEASES_DIR}"

echo "Building native tools..."
cmake -S . -B build -G Ninja
cmake --build build

rm -rf "${RELEASE_DIR}"
mkdir -p "${BIN_DIR}" "${CONFIG_DIR}" "${LAUNCHER_DIR}" "${DOCS_DIR}" "${GUI_DIR}" "${SESSIONS_DIR}"

echo
echo "Copying native executables..."
if [ ! -d "${BUILD_DIR}" ]; then
    echo "ERROR: Native build folder not found: ${BUILD_DIR}"
    echo "Expected executables under build/native/"
    exit 1
fi

cp -v "${BUILD_DIR}"/*.exe "${BIN_DIR}/"

copy_dll_deps() {
    local exe="$1"

    if ! command -v ldd >/dev/null 2>&1; then
        return 0
    fi

    ldd "$exe" 2>/dev/null \
        | awk '
            {
                for (i = 1; i <= NF; i++) {
                    if ($i ~ /^\// && $i ~ /\.dll$/) {
                        print $i
                    }
                }
            }
        ' \
        | sort -u \
        | while read -r dll; do
            if [ -f "$dll" ]; then
                cp -u "$dll" "${BIN_DIR}/"
            fi
        done
}

echo
echo "Copying MSYS2/UCRT64 DLL dependencies..."
for exe in "${BIN_DIR}"/*.exe; do
    copy_dll_deps "$exe"
done

echo
echo "Copying configs..."
if [ -d "${ROOT}/configs" ]; then
    cp -rv "${ROOT}/configs/"* "${CONFIG_DIR}/"
fi

echo
echo "Copying docs..."
if [ -f "${ROOT}/README.md" ]; then
    cp -v "${ROOT}/README.md" "${RELEASE_DIR}/README.md"
fi

if [ -d "${ROOT}/docs" ]; then
    cp -rv "${ROOT}/docs/"* "${DOCS_DIR}/" || true
fi

cat > "${RELEASE_DIR}/README_RELEASE.txt" <<'EOF'
Pluto+ SDR Windows Toolkit Release
==================================

Folder layout:

  bin/native/     Native command-line tools and DLL dependencies
  configs/        Scan session config profiles
  launchers/      Double-click Windows launchers
  gui/            Published WPF GUI apps, if available
  docs/           Documentation
  sessions/       Generated CSV and HTML reports go here

Quick start:

  1. Connect the Pluto+ SDR.
  2. Open launchers/run_noaa_scan.cmd or launchers/run_fm_scan.cmd.
  3. Reports are created under sessions/.
  4. Open the generated *_report.html file.

Native examples from a command prompt:

  bin\native\pluto_scan_session.exe --config configs\2m.conf
  bin\native\pluto_scan_session.exe --config configs\fm.conf
  bin\native\pluto_spectrum_stream.exe --freq 100000000 --rate 2000000 --fft 2048

Notes:

  This release assumes your Pluto+ supports RX coverage down to at least 70 MHz.
  FM broadcast, 88-108 MHz, is included as a standard test target.

  If a native EXE reports missing DLLs, install MSYS2 UCRT64 on that machine or
  copy any missing DLLs from C:\msys64\ucrt64\bin into bin\native.
EOF

echo
echo "Creating release launchers..."
cat > "${LAUNCHER_DIR}/run_profile.cmd" <<'EOF'
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
echo.
pause
EOF

make_launcher() {
    local name="$1"
    local config="$2"
    local prefix="$3"
    local label="$4"

    cat > "${LAUNCHER_DIR}/${name}" <<EOF
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

make_launcher "run_fm_scan.cmd" "fm.conf" "fm" "FM broadcast"
make_launcher "run_2m_scan.cmd" "2m.conf" "2m" "2 meter"
make_launcher "run_airband_scan.cmd" "airband.conf" "airband" "airband"
make_launcher "run_noaa_scan.cmd" "noaa.conf" "noaa" "NOAA"
make_launcher "run_70cm_scan.cmd" "70cm.conf" "70cm" "70 cm"

cat > "${LAUNCHER_DIR}/open_sessions_folder.cmd" <<'EOF'
@echo off
setlocal
set "RELEASE_ROOT=%~dp0.."
mkdir "%RELEASE_ROOT%\sessions" 2>nul
start "" "%RELEASE_ROOT%\sessions"
EOF

cat > "${LAUNCHER_DIR}/start_live_spectrum_fm.cmd" <<'EOF'
@echo off
setlocal
set "RELEASE_ROOT=%~dp0.."
set "BIN_DIR=%RELEASE_ROOT%\bin\native"
set "PATH=%BIN_DIR%;%PATH%"
"%BIN_DIR%\pluto_spectrum_stream.exe" --freq 100000000 --rate 2000000 --fft 2048 --avg 2
pause
EOF

echo
echo "Publishing WPF GUIs if source projects are present..."
publish_gui() {
    local project_dir="$1"
    local out_name="$2"

    if [ -f "${project_dir}"/*.csproj ]; then
        echo "Publishing ${out_name}..."
        dotnet publish "${project_dir}" -c Release -o "${GUI_DIR}/${out_name}" --self-contained false || {
            echo "WARNING: dotnet publish failed for ${out_name}; continuing without it."
        }
    fi
}

# Known GUI folders from this project history.
if command -v dotnet >/dev/null 2>&1; then
    publish_gui "${ROOT}/gui/PlutoGuiStarter_v3_RepoLayout" "PlutoSessionGui"
    publish_gui "${ROOT}/gui/PlutoLiveSpectrumGui_v5_RepoLayout_FM" "PlutoLiveSpectrumGui"

    # Fallback names if the user kept older folders.
    if [ ! -d "${GUI_DIR}/PlutoSessionGui" ]; then
        publish_gui "${ROOT}/gui/PlutoGuiStarter" "PlutoSessionGui"
    fi

    if [ ! -d "${GUI_DIR}/PlutoLiveSpectrumGui" ]; then
        publish_gui "${ROOT}/gui/PlutoLiveSpectrumGui" "PlutoLiveSpectrumGui"
    fi
else
    echo "dotnet not found; skipping GUI publish."
fi

cat > "${LAUNCHER_DIR}/start_session_gui.cmd" <<'EOF'
@echo off
setlocal
set "RELEASE_ROOT=%~dp0.."

if exist "%RELEASE_ROOT%\gui\PlutoSessionGui\PlutoGuiStarter.exe" (
    start "" "%RELEASE_ROOT%\gui\PlutoSessionGui\PlutoGuiStarter.exe"
    exit /b 0
)

echo Session GUI executable was not found.
echo Expected:
echo   %RELEASE_ROOT%\gui\PlutoSessionGui\PlutoGuiStarter.exe
pause
EOF

cat > "${LAUNCHER_DIR}/start_live_spectrum_gui.cmd" <<'EOF'
@echo off
setlocal
set "RELEASE_ROOT=%~dp0.."

if exist "%RELEASE_ROOT%\gui\PlutoLiveSpectrumGui\PlutoLiveSpectrumGui.exe" (
    start "" "%RELEASE_ROOT%\gui\PlutoLiveSpectrumGui\PlutoLiveSpectrumGui.exe"
    exit /b 0
)

echo Live Spectrum GUI executable was not found.
echo Expected:
echo   %RELEASE_ROOT%\gui\PlutoLiveSpectrumGui\PlutoLiveSpectrumGui.exe
pause
EOF

echo
echo "Writing release manifest..."
{
    echo "Pluto+ SDR Windows Toolkit Release"
    echo "Release: ${RELEASE_NAME}"
    echo "Created: $(date)"
    echo
    echo "Native EXEs:"
    find "${BIN_DIR}" -maxdepth 1 -name "*.exe" -printf "  %f\n" | sort
    echo
    echo "Configs:"
    find "${CONFIG_DIR}" -maxdepth 1 -type f -printf "  %f\n" | sort
    echo
    echo "Launchers:"
    find "${LAUNCHER_DIR}" -maxdepth 1 -type f -printf "  %f\n" | sort
} > "${RELEASE_DIR}/MANIFEST.txt"

echo
echo "Creating ZIP..."
(
    cd "${RELEASES_DIR}"
    rm -f "${RELEASE_NAME}.zip"
    zip -r "${RELEASE_NAME}.zip" "${RELEASE_NAME}"
)

echo
echo "Release package complete:"
echo "  ${RELEASE_DIR}"
echo "  ${RELEASES_DIR}/${RELEASE_NAME}.zip"
