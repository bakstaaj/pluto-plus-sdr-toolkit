#!/usr/bin/env bash
set -Eeuo pipefail

# repair_audio_menu_launcher.sh
#
# Repairs repo-level and release-level run_audio_menu.cmd quoting.
#
# Assumes package extracted to:
#   C:\Users\jim\Downloads\PlutoAudioMenuQuotingFix
#
# Usage from repo root:
#
#   ./tools/repair_audio_menu_launcher.sh
#   ./tools/repair_audio_menu_launcher.sh releases/pluto-plus-sdr-toolkit-v1.1-audio-menu

PACKAGE_ROOT_WINDOWS="${PACKAGE_ROOT_WINDOWS:-C:\\Users\\jim\\Downloads\\PlutoAudioMenuQuotingFix}"

if command -v cygpath >/dev/null 2>&1; then
    PACKAGE_ROOT="$(cygpath -u "$PACKAGE_ROOT_WINDOWS")"
else
    PACKAGE_ROOT="$PACKAGE_ROOT_WINDOWS"
fi

if [ ! -d "$PACKAGE_ROOT" ]; then
    echo "ERROR: Package folder not found:"
    echo "  $PACKAGE_ROOT_WINDOWS"
    exit 1
fi

mkdir -p launchers

cp "$PACKAGE_ROOT/launchers/run_audio_menu.cmd" launchers/run_audio_menu.cmd
echo "Updated repo launcher:"
echo "  launchers/run_audio_menu.cmd"

if [ "${1:-}" != "" ]; then
    RELEASE_DIR="$1"
    if [ ! -d "$RELEASE_DIR" ]; then
        echo "ERROR: release folder does not exist:"
        echo "  $RELEASE_DIR"
        exit 1
    fi

    mkdir -p "$RELEASE_DIR/launchers"
    cp "$PACKAGE_ROOT/launchers/release_run_audio_menu.cmd" "$RELEASE_DIR/launchers/run_audio_menu.cmd"
    echo "Updated release launcher:"
    echo "  $RELEASE_DIR/launchers/run_audio_menu.cmd"
fi

echo
echo "Done."
