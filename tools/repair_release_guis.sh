#!/usr/bin/env bash
set -Eeuo pipefail

# repair_release_guis.sh
#
# Publishes WPF GUI apps into an existing release folder and repairs the GUI launchers.
#
# Usage from repo root:
#
#   ./tools/repair_release_guis.sh releases/pluto-plus-sdr-toolkit-v0.5-clean-layout
#
# If no release folder is passed, uses the newest releases/pluto-plus-sdr-toolkit-* folder.

ROOT="$(pwd)"
RELEASE_DIR="${1:-}"

if [ -z "$RELEASE_DIR" ]; then
    RELEASE_DIR="$(find "${ROOT}/releases" -maxdepth 1 -type d -name 'pluto-plus-sdr-toolkit-*' | sort | tail -n 1 || true)"
fi

if [ -z "$RELEASE_DIR" ] || [ ! -d "$RELEASE_DIR" ]; then
    echo "ERROR: release folder not found."
    echo "Usage:"
    echo "  ./tools/repair_release_guis.sh releases/pluto-plus-sdr-toolkit-v0.5-clean-layout"
    exit 1
fi

GUI_DIR="${RELEASE_DIR}/gui"
LAUNCHER_DIR="${RELEASE_DIR}/launchers"

mkdir -p "$GUI_DIR" "$LAUNCHER_DIR"

find_dotnet() {
    if command -v dotnet >/dev/null 2>&1; then
        command -v dotnet
        return 0
    fi

    for candidate in \
        "/c/Program Files/dotnet/dotnet.exe" \
        "/c/Program Files (x86)/dotnet/dotnet.exe" \
        "/mnt/c/Program Files/dotnet/dotnet.exe" \
        "/mnt/c/Program Files (x86)/dotnet/dotnet.exe"
    do
        if [ -x "$candidate" ]; then
            echo "$candidate"
            return 0
        fi
    done

    return 1
}

publish_gui() {
    local source_dir="$1"
    local output_name="$2"

    if [ ! -d "$source_dir" ]; then
        echo "WARNING: GUI source folder not found:"
        echo "  $source_dir"
        return 0
    fi

    local csproj
    csproj="$(find "$source_dir" -maxdepth 1 -name '*.csproj' | head -n 1 || true)"

    if [ -z "$csproj" ]; then
        echo "WARNING: no .csproj found in:"
        echo "  $source_dir"
        return 0
    fi

    echo
    echo "Publishing ${output_name}"
    echo "  Project: $csproj"
    echo "  Output:  ${GUI_DIR}/${output_name}"

    "$DOTNET_EXE" publish "$csproj" -c Release -o "${GUI_DIR}/${output_name}" --self-contained false

    echo "Published files:"
    find "${GUI_DIR}/${output_name}" -maxdepth 1 -type f -printf "  %f\n" | sort
}

write_gui_launchers() {
    cat > "${LAUNCHER_DIR}/start_session_gui.cmd" <<'EOF'
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

    cat > "${LAUNCHER_DIR}/start_live_spectrum_gui.cmd" <<'EOF'
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
}

if ! DOTNET_EXE="$(find_dotnet)"; then
    echo "ERROR: dotnet was not found."
    echo
    echo "From PowerShell, verify:"
    echo "  dotnet --version"
    echo
    echo "If PowerShell has dotnet but MSYS2 does not, this script looks in:"
    echo "  /c/Program Files/dotnet/dotnet.exe"
    exit 1
fi

echo "Using dotnet:"
echo "  $DOTNET_EXE"
echo
echo "Repairing release:"
echo "  $RELEASE_DIR"

publish_gui "${ROOT}/gui/PlutoGuiStarter_v3_RepoLayout" "PlutoSessionGui"
publish_gui "${ROOT}/gui/PlutoLiveSpectrumGui_v5_RepoLayout_FM" "PlutoLiveSpectrumGui"

echo
echo "Writing flexible GUI launchers..."
write_gui_launchers

echo
echo "Verifying GUI publish:"
session_count="$(find "${GUI_DIR}/PlutoSessionGui" -maxdepth 1 -type f -name '*.exe' 2>/dev/null | wc -l | tr -d ' ')"
spectrum_count="$(find "${GUI_DIR}/PlutoLiveSpectrumGui" -maxdepth 1 -type f -name '*.exe' 2>/dev/null | wc -l | tr -d ' ')"

echo "  Session GUI EXEs:       ${session_count}"
echo "  Live Spectrum GUI EXEs: ${spectrum_count}"

if [ "$session_count" -eq 0 ]; then
    echo "ERROR: Session GUI publish did not produce an .exe."
    exit 1
fi

if [ "$spectrum_count" -eq 0 ]; then
    echo "ERROR: Live Spectrum GUI publish did not produce an .exe."
    exit 1
fi

echo
echo "GUI repair complete."
echo
echo "Test these from Windows Explorer:"
echo "  ${LAUNCHER_DIR}/start_session_gui.cmd"
echo "  ${LAUNCHER_DIR}/start_live_spectrum_gui.cmd"
