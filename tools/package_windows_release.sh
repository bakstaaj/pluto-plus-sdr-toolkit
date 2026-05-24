#!/usr/bin/env bash
set -Eeuo pipefail

# package_windows_release.sh
#
# Consolidated Windows release packager for the Pluto+ SDR Windows Toolkit.
#
# WBFM release integration:
#   - packages pluto_audio_monitor.exe
#   - creates run_noaa_audio.cmd
#   - creates run_airband_audio.cmd
#   - creates run_fm_audio.cmd
#   - verifies all audio launchers exist

APP_NAME="pluto-plus-sdr-toolkit"
VERSION="${1:-$(date +%Y%m%d-%H%M%S)}"
RELEASE_NAME="${APP_NAME}-${VERSION}"

ROOT="$(pwd)"
RELEASES_DIR="${ROOT}/releases"
RELEASE_DIR="${RELEASES_DIR}/${RELEASE_NAME}"
BUILD_DIR="${ROOT}/build/native"

BIN_DIR="${RELEASE_DIR}/bin/native"
CONFIG_DIR="${RELEASE_DIR}/configs"
DOCS_DIR="${RELEASE_DIR}/docs"
GUI_DIR="${RELEASE_DIR}/gui"
SESSIONS_DIR="${RELEASE_DIR}/sessions"
LAUNCHER_DIR="${RELEASE_DIR}/launchers"

die() {
    echo
    echo "ERROR: $*" >&2
    exit 1
}

same_file() {
    local a="$1"
    local b="$2"

    if command -v realpath >/dev/null 2>&1; then
        [ "$(realpath -m "$a")" = "$(realpath -m "$b")" ]
        return $?
    fi

    [ "$a" = "$b" ]
}

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

copy_native_dll_dependencies() {
    local exe="$1"

    if ! command -v ldd >/dev/null 2>&1; then
        echo "  ldd not found; skipping DLL dependency copy for $(basename "$exe")"
        return 0
    fi

    echo "  scanning $(basename "$exe")"

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
            if [ ! -f "$dll" ]; then
                continue
            fi

            local dest="${BIN_DIR}/$(basename "$dll")"

            if same_file "$dll" "$dest"; then
                echo "    skip already local: $(basename "$dll")"
                continue
            fi

            if [ -f "$dest" ]; then
                echo "    already copied: $(basename "$dll")"
                continue
            fi

            cp "$dll" "$dest"
            echo "    copied: $(basename "$dll")"
        done
}

publish_gui_project() {
    local dotnet_exe="$1"
    local project_dir="$2"
    local out_name="$3"

    if [ ! -d "$project_dir" ]; then
        echo "  GUI source folder not found, skipping: ${project_dir}"
        return 0
    fi

    local csproj
    csproj="$(find "$project_dir" -maxdepth 1 -name "*.csproj" | head -n 1 || true)"

    if [ -z "$csproj" ]; then
        echo "  No .csproj found in ${project_dir}; skipping."
        return 0
    fi

    if [ -z "$dotnet_exe" ]; then
        echo "  dotnet not found; skipping ${out_name} publish."
        return 0
    fi

    echo "  Publishing ${out_name}"
    echo "    dotnet: ${dotnet_exe}"
    echo "    project: ${csproj}"

    "$dotnet_exe" publish "$csproj" -c Release -o "${GUI_DIR}/${out_name}" --self-contained false || {
        echo "  WARNING: dotnet publish failed for ${out_name}; continuing without it."
        return 0
    }

    local exe_count
    exe_count="$(find "${GUI_DIR}/${out_name}" -maxdepth 1 -type f -name "*.exe" | wc -l | tr -d ' ')"
    echo "    GUI EXEs published: ${exe_count}"
}

create_zip() {
    local zip_path="${RELEASES_DIR}/${RELEASE_NAME}.zip"

    rm -f "$zip_path"

    (
        cd "$RELEASES_DIR"

        if command -v zip >/dev/null 2>&1; then
            zip -r "${RELEASE_NAME}.zip" "${RELEASE_NAME}"
        else
            powershell.exe -NoProfile -Command "Compress-Archive -Path '${RELEASE_NAME}' -DestinationPath '${RELEASE_NAME}.zip' -Force"
        fi
    )
}

verify_release() {
    echo
    echo "Verifying release package..."

    [ -d "$BIN_DIR" ] || die "Missing bin/native folder."
    [ -d "$CONFIG_DIR" ] || die "Missing configs folder."
    [ -d "$LAUNCHER_DIR" ] || die "Missing launchers folder."
    [ -d "$SESSIONS_DIR" ] || die "Missing sessions folder."

    local exe_count
    exe_count="$(find "$BIN_DIR" -maxdepth 1 -type f -name "*.exe" | wc -l | tr -d ' ')"

    local config_count
    config_count="$(find "$CONFIG_DIR" -maxdepth 1 -type f -name "*.conf" | wc -l | tr -d ' ')"

    local launcher_count
    launcher_count="$(find "$LAUNCHER_DIR" -maxdepth 1 -type f -name "*.cmd" | wc -l | tr -d ' ')"

    local session_gui_count=0
    local live_gui_count=0

    if [ -d "${GUI_DIR}/PlutoSessionGui" ]; then
        session_gui_count="$(find "${GUI_DIR}/PlutoSessionGui" -maxdepth 1 -type f -name "*.exe" | wc -l | tr -d ' ')"
    fi

    if [ -d "${GUI_DIR}/PlutoLiveSpectrumGui" ]; then
        live_gui_count="$(find "${GUI_DIR}/PlutoLiveSpectrumGui" -maxdepth 1 -type f -name "*.exe" | wc -l | tr -d ' ')"
    fi

    echo "  Native EXEs:        ${exe_count}"
    echo "  Configs:            ${config_count}"
    echo "  Launchers:          ${launcher_count}"
    echo "  Session GUI EXEs:   ${session_gui_count}"
    echo "  Spectrum GUI EXEs:  ${live_gui_count}"

    [ "$exe_count" -gt 0 ] || die "No native EXEs were packaged."
    [ "$config_count" -gt 0 ] || die "No config files were packaged."
    [ "$launcher_count" -ge 13 ] || die "Expected at least 13 launchers, found ${launcher_count}."

    for required in \
        pluto_scan_session.exe \
        pluto_spectrum_stream.exe
    do
        [ -f "${BIN_DIR}/${required}" ] || die "Missing native tool: ${required}"
    done

    if [ -f "${ROOT}/native/src/pluto_audio_monitor.c" ]; then
        [ -f "${BIN_DIR}/pluto_audio_monitor.exe" ] || die "Missing audio tool: pluto_audio_monitor.exe"
        [ -f "${LAUNCHER_DIR}/run_noaa_audio.cmd" ] || die "Missing audio launcher: run_noaa_audio.cmd"
        [ -f "${LAUNCHER_DIR}/run_airband_audio.cmd" ] || die "Missing audio launcher: run_airband_audio.cmd"
        [ -f "${LAUNCHER_DIR}/run_fm_audio.cmd" ] || die "Missing audio launcher: run_fm_audio.cmd"
    fi

    for required in \
        run_fm_scan.cmd \
        run_2m_scan.cmd \
        run_noaa_scan.cmd \
        start_session_gui.cmd \
        start_live_spectrum_gui.cmd
    do
        [ -f "${LAUNCHER_DIR}/${required}" ] || die "Missing launcher: ${required}"
    done

    if [ "$session_gui_count" -eq 0 ]; then
        echo "  WARNING: Session GUI was not published."
    fi

    if [ "$live_gui_count" -eq 0 ]; then
        echo "  WARNING: Live Spectrum GUI was not published."
    fi

    echo "  Release verification passed."
}

echo "Pluto+ SDR Windows release packager"
echo "Root:    ${ROOT}"
echo "Release: ${RELEASE_DIR}"
echo

[ -f "${ROOT}/tools/create_release_launchers.sh" ] || die "Missing tools/create_release_launchers.sh."

DOTNET_EXE=""
if DOTNET_EXE="$(find_dotnet)"; then
    echo "Using dotnet: ${DOTNET_EXE}"
else
    echo "WARNING: dotnet was not found. GUI publishing will be skipped."
fi

echo
echo "Building native tools..."
cmake -S . -B build -G Ninja
cmake --build build

[ -d "$BUILD_DIR" ] || die "Native build folder not found: ${BUILD_DIR}"

mkdir -p "$RELEASES_DIR"
rm -rf "$RELEASE_DIR"

mkdir -p \
    "$BIN_DIR" \
    "$CONFIG_DIR" \
    "$DOCS_DIR" \
    "$GUI_DIR" \
    "$SESSIONS_DIR" \
    "$LAUNCHER_DIR"

echo
echo "Copying native executables..."
shopt -s nullglob
native_exes=("${BUILD_DIR}"/*.exe)
[ "${#native_exes[@]}" -gt 0 ] || die "No native EXEs found in ${BUILD_DIR}"

cp -v "${native_exes[@]}" "$BIN_DIR/"

echo
echo "Copying MSYS2/UCRT64 DLL dependencies..."
for exe in "${BIN_DIR}"/*.exe; do
    copy_native_dll_dependencies "$exe"
done

echo
echo "Copying configs..."
[ -d "${ROOT}/configs" ] || die "configs folder not found."
cp -rv "${ROOT}/configs/"* "$CONFIG_DIR/" || true

echo
echo "Copying documentation..."
if [ -f "${ROOT}/README.md" ]; then
    cp -v "${ROOT}/README.md" "${RELEASE_DIR}/README.md"
fi

if [ -d "${ROOT}/docs" ]; then
    cp -rv "${ROOT}/docs/"* "$DOCS_DIR/" || true
fi

cat > "${RELEASE_DIR}/README_RELEASE.txt" <<'EOF'
Pluto+ SDR Windows Toolkit Release
==================================

Quick start:

  launchers\run_noaa_scan.cmd
  launchers\run_fm_scan.cmd
  launchers\run_noaa_audio.cmd
  launchers\run_airband_audio.cmd
  launchers\run_fm_audio.cmd
  launchers\start_session_gui.cmd
  launchers\start_live_spectrum_gui.cmd

Folder layout:

  bin\native\     Native command-line tools and DLL dependencies
  configs\        Scan session config profiles
  launchers\      Double-click Windows launchers
  gui\            Published WPF GUI apps, if available
  docs\           Documentation
  sessions\       Generated CSV, WAV, and HTML reports go here

Generated reports, WAV files, and CSV files are written to:

  sessions\

Hardware assumption:

  This release assumes your Pluto+ supports RX coverage down to at least 70 MHz.
  FM broadcast, 88-108 MHz, is included as a standard test target.
EOF

echo
echo "Publishing WPF GUIs..."
publish_gui_project "$DOTNET_EXE" "${ROOT}/gui/PlutoGuiStarter_v3_RepoLayout" "PlutoSessionGui"
publish_gui_project "$DOTNET_EXE" "${ROOT}/gui/PlutoLiveSpectrumGui_v5_RepoLayout_FM" "PlutoLiveSpectrumGui"

echo
echo "Creating launchers..."
bash "${ROOT}/tools/create_release_launchers.sh" "$RELEASE_DIR"

echo
echo "Writing manifest..."
{
    echo "Pluto+ SDR Windows Toolkit Release"
    echo "Release: ${RELEASE_NAME}"
    echo "Created: $(date)"
    echo "dotnet: ${DOTNET_EXE:-not found}"
    echo
    echo "Native EXEs:"
    find "$BIN_DIR" -maxdepth 1 -name "*.exe" -printf "  %f\n" | sort
    echo
    echo "Native DLLs:"
    find "$BIN_DIR" -maxdepth 1 -name "*.dll" -printf "  %f\n" | sort
    echo
    echo "Config files:"
    find "$CONFIG_DIR" -maxdepth 1 -name "*.conf" -printf "  %f\n" | sort
    echo
    echo "Launchers:"
    find "$LAUNCHER_DIR" -maxdepth 1 -name "*.cmd" -printf "  %f\n" | sort
    echo
    echo "Session GUI files:"
    if [ -d "${GUI_DIR}/PlutoSessionGui" ]; then
        find "${GUI_DIR}/PlutoSessionGui" -maxdepth 1 -type f -printf "  %f\n" | sort
    else
        echo "  not published"
    fi
    echo
    echo "Live Spectrum GUI files:"
    if [ -d "${GUI_DIR}/PlutoLiveSpectrumGui" ]; then
        find "${GUI_DIR}/PlutoLiveSpectrumGui" -maxdepth 1 -type f -printf "  %f\n" | sort
    else
        echo "  not published"
    fi
} > "${RELEASE_DIR}/MANIFEST.txt"

verify_release

echo
echo "Creating ZIP archive..."
create_zip

echo
echo "Release package complete:"
echo "  ${RELEASE_DIR}"
echo "  ${RELEASES_DIR}/${RELEASE_NAME}.zip"
