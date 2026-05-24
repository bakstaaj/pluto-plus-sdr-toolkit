#!/usr/bin/env bash
set -Eeuo pipefail

PACKAGE_ROOT_WINDOWS="${PACKAGE_ROOT_WINDOWS:-C:\\Users\\jim\\Downloads\\PlutoGuiAudioTools}"
GUI_DIR="${GUI_DIR:-gui/PlutoGuiStarter_v3_RepoLayout}"

if command -v cygpath >/dev/null 2>&1; then
    PACKAGE_ROOT="$(cygpath -u "$PACKAGE_ROOT_WINDOWS")"
else
    PACKAGE_ROOT="$PACKAGE_ROOT_WINDOWS"
fi

if [ ! -d "$PACKAGE_ROOT" ]; then
    echo "ERROR: Package folder was not found."
    echo "Expected: $PACKAGE_ROOT_WINDOWS"
    exit 1
fi

if [ ! -d "$GUI_DIR" ]; then
    echo "ERROR: GUI folder not found: $GUI_DIR"
    echo "Override with: GUI_DIR=gui/YourGuiFolder ./tools/install_gui_audio_tools.sh"
    exit 1
fi

mkdir -p tools docs
cp "$PACKAGE_ROOT/tools/patch_gui_audio_tools.py" tools/patch_gui_audio_tools.py
cp "$PACKAGE_ROOT/docs/GUI_AUDIO_TOOLS.md" docs/GUI_AUDIO_TOOLS.md
chmod +x tools/patch_gui_audio_tools.py
python3 tools/patch_gui_audio_tools.py --gui-dir "$GUI_DIR"

echo
echo "Build GUI:"
echo "  cd $GUI_DIR"
echo "  dotnet build"
echo
echo "Run GUI:"
echo "  dotnet run"
