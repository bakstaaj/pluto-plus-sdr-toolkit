#!/usr/bin/env bash
set -Eeuo pipefail

# write_release_audio_menu.sh
#
# Writes the report-aware run_audio_menu.cmd into an existing release folder.
#
# Usage:
#   ./tools/write_release_audio_menu.sh releases/pluto-plus-sdr-toolkit-v1.3-audio-report

PACKAGE_ROOT_WINDOWS="${PACKAGE_ROOT_WINDOWS:-C:\\Users\\jim\\Downloads\\PlutoAudioMenuReportWorkflow}"

if command -v cygpath >/dev/null 2>&1; then
    PACKAGE_ROOT="$(cygpath -u "$PACKAGE_ROOT_WINDOWS")"
else
    PACKAGE_ROOT="$PACKAGE_ROOT_WINDOWS"
fi

RELEASE_DIR="${1:-}"

if [ -z "$RELEASE_DIR" ] || [ ! -d "$RELEASE_DIR" ]; then
    echo "ERROR: release folder argument is required and must exist."
    exit 1
fi

if [ ! -f "$PACKAGE_ROOT/launchers/release_run_audio_menu.cmd" ]; then
    echo "ERROR: release menu template not found:"
    echo "  $PACKAGE_ROOT/launchers/release_run_audio_menu.cmd"
    exit 1
fi

mkdir -p "$RELEASE_DIR/launchers"
cp "$PACKAGE_ROOT/launchers/release_run_audio_menu.cmd" "$RELEASE_DIR/launchers/run_audio_menu.cmd"

echo "Updated:"
echo "  $RELEASE_DIR/launchers/run_audio_menu.cmd"
