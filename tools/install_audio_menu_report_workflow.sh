#!/usr/bin/env bash
set -Eeuo pipefail

# install_audio_menu_report_workflow.sh
#
# Run from repository root:
#   cd ~/sdrdev/pluto_native_test
#   ./tools/install_audio_menu_report_workflow.sh
#
# Assumes package extracted to:
#   C:\Users\jim\Downloads\PlutoAudioMenuReportWorkflow

PACKAGE_ROOT_WINDOWS="${PACKAGE_ROOT_WINDOWS:-C:\\Users\\jim\\Downloads\\PlutoAudioMenuReportWorkflow}"

if command -v cygpath >/dev/null 2>&1; then
    PACKAGE_ROOT="$(cygpath -u "$PACKAGE_ROOT_WINDOWS")"
else
    PACKAGE_ROOT="$PACKAGE_ROOT_WINDOWS"
fi

if [ ! -d "$PACKAGE_ROOT" ]; then
    echo "ERROR: Package folder was not found."
    echo "Expected:"
    echo "  $PACKAGE_ROOT_WINDOWS"
    exit 1
fi

mkdir -p launchers tools docs

cp "$PACKAGE_ROOT/launchers/run_audio_menu.cmd" launchers/run_audio_menu.cmd
cp "$PACKAGE_ROOT/launchers/release_run_audio_menu.cmd" launchers/release_run_audio_menu.cmd
cp "$PACKAGE_ROOT/tools/write_release_audio_menu.sh" tools/write_release_audio_menu.sh
cp "$PACKAGE_ROOT/docs/AUDIO_MENU_REPORT_WORKFLOW.md" docs/AUDIO_MENU_REPORT_WORKFLOW.md

chmod +x tools/write_release_audio_menu.sh

echo "Installed audio menu report workflow files."
echo
echo "Test repo menu:"
echo "  cmd.exe /c launchers\\\\run_audio_menu.cmd"
echo
echo "Update an existing release menu:"
echo "  ./tools/write_release_audio_menu.sh releases/pluto-plus-sdr-toolkit-v1.3-audio-report"
