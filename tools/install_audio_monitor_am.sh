#!/usr/bin/env bash
set -Eeuo pipefail

# install_audio_monitor_am.sh
#
# Run from repository root:
#   cd ~/sdrdev/pluto_native_test
#   ./tools/install_audio_monitor_am.sh
#
# This script assumes you extracted the package to:
#   C:\Users\jim\Downloads\PlutoAudioMonitorAM

PACKAGE_ROOT_WINDOWS="${PACKAGE_ROOT_WINDOWS:-C:\\Users\\jim\\Downloads\\PlutoAudioMonitorAM}"

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
    echo "Extract PlutoAudioMonitorAM.zip into:"
    echo "  C:\\Users\\jim\\Downloads\\PlutoAudioMonitorAM"
    exit 1
fi

echo "Using package folder:"
echo "  $PACKAGE_ROOT"
echo

mkdir -p native/src docs launchers tools sessions

cp "$PACKAGE_ROOT/native/src/pluto_audio_monitor.c" native/src/pluto_audio_monitor.c
cp "$PACKAGE_ROOT/docs/AUDIO_MONITOR.md" docs/AUDIO_MONITOR.md
cp "$PACKAGE_ROOT/docs/AIRBAND_AM_AUDIO.md" docs/AIRBAND_AM_AUDIO.md
cp "$PACKAGE_ROOT/launchers/run_airband_audio.cmd" launchers/run_airband_audio.cmd

if ! grep -q "pluto_audio_monitor" native/CMakeLists.txt; then
    cat >> native/CMakeLists.txt <<'EOF'

add_executable(pluto_audio_monitor src/pluto_audio_monitor.c)
target_link_libraries(pluto_audio_monitor PRIVATE PkgConfig::LIBIIO PkgConfig::LIBAD9361 m)

if (MINGW)
    target_compile_options(pluto_audio_monitor PRIVATE -Wall -Wextra -O2)
endif()
EOF
    echo "Added pluto_audio_monitor target to native/CMakeLists.txt"
else
    echo "native/CMakeLists.txt already references pluto_audio_monitor"
fi

echo
echo "Install complete."
echo
echo "Build:"
echo "  ./tools/build_native_ucrt64.sh"
echo
echo "Airband AM test:"
echo "  mkdir -p sessions"
echo "  ./build/native/pluto_audio_monitor.exe --mode am --preset airband-125 --seconds 20 --squelch-db -65 --wav sessions/airband_am_test.wav --csv sessions/audio_log.csv"
