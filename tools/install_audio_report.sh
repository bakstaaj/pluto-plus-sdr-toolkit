#!/usr/bin/env bash
set -Eeuo pipefail

# install_audio_report.sh
#
# Run from repository root:
#   cd ~/sdrdev/pluto_native_test
#   ./tools/install_audio_report.sh
#
# This script assumes you extracted the package to:
#   C:\Users\jim\Downloads\PlutoAudioReportGenerator

PACKAGE_ROOT_WINDOWS="${PACKAGE_ROOT_WINDOWS:-C:\\Users\\jim\\Downloads\\PlutoAudioReportGenerator}"

if command -v cygpath >/dev/null 2>&1; then
    PACKAGE_ROOT="$(cygpath -u "$PACKAGE_ROOT_WINDOWS")"
else
    PACKAGE_ROOT="$PACKAGE_ROOT_WINDOWS"
fi

if [ ! -d "$PACKAGE_ROOT" ]; then
    echo "ERROR: Package folder was not found."
    echo
    echo "Expected:"
    echo "  $PACKAGE_ROOT_WINDOWS"
    echo
    echo "Extract PlutoAudioReportGenerator.zip into:"
    echo "  C:\\Users\\jim\\Downloads\\PlutoAudioReportGenerator"
    exit 1
fi

echo "Using package folder:"
echo "  $PACKAGE_ROOT"
echo

mkdir -p native/src docs launchers tools sessions

cp "$PACKAGE_ROOT/native/src/pluto_audio_report.c" native/src/pluto_audio_report.c
cp "$PACKAGE_ROOT/docs/AUDIO_REPORT.md" docs/AUDIO_REPORT.md
cp "$PACKAGE_ROOT/launchers/make_audio_report.cmd" launchers/make_audio_report.cmd

if ! grep -q "pluto_audio_report" native/CMakeLists.txt; then
    cat >> native/CMakeLists.txt <<'EOF'

add_executable(pluto_audio_report src/pluto_audio_report.c)

if (MINGW)
    target_compile_options(pluto_audio_report PRIVATE -Wall -Wextra -O2)
endif()
EOF
    echo "Added pluto_audio_report target to native/CMakeLists.txt"
else
    echo "native/CMakeLists.txt already references pluto_audio_report"
fi

echo
echo "Install complete."
echo
echo "Build:"
echo "  ./tools/build_native_ucrt64.sh"
echo
echo "Generate report:"
echo "  ./build/native/pluto_audio_report.exe --in sessions/audio_log.csv --out sessions/audio_report.html"
echo
echo "Open report:"
echo "  explorer.exe sessions/audio_report.html"
