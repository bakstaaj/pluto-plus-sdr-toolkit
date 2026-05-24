#!/usr/bin/env bash
set -Eeuo pipefail

# install_audio_monitor.sh
#
# Run from repository root:
#   cd ~/sdrdev/pluto_native_test
#   ./tools/install_audio_monitor.sh
#
# This script assumes you extracted the package to:
#   C:\Users\jim\Downloads\PlutoAudioMonitorUpdate
#
# Override if needed:
#   PACKAGE_ROOT_WINDOWS='C:\Users\jim\Downloads\PlutoAudioMonitorUpdate' ./tools/install_audio_monitor.sh

PACKAGE_ROOT_WINDOWS="${PACKAGE_ROOT_WINDOWS:-C:\\Users\\jim\\Downloads\\PlutoAudioMonitorUpdate}"

if command -v cygpath >/dev/null 2>&1; then
    PACKAGE_ROOT="$(cygpath -u "$PACKAGE_ROOT_WINDOWS")"
else
    # MSYS2 normally has cygpath. This fallback handles forward-slash Windows paths.
    PACKAGE_ROOT="$PACKAGE_ROOT_WINDOWS"
fi

if [ ! -d "$PACKAGE_ROOT" ]; then
    echo "ERROR: Package folder was not found."
    echo
    echo "Expected Windows path:"
    echo "  $PACKAGE_ROOT_WINDOWS"
    echo
    echo "Expected MSYS2 path:"
    echo "  $PACKAGE_ROOT"
    echo
    echo "Extract PlutoAudioMonitorUpdate.zip into:"
    echo "  C:\\Users\\jim\\Downloads\\PlutoAudioMonitorUpdate"
    echo
    echo "Or override the package location:"
    echo "  PACKAGE_ROOT_WINDOWS='C:\\Users\\jim\\Downloads\\PlutoAudioMonitorUpdate' ./tools/install_audio_monitor.sh"
    exit 1
fi

echo "Using package folder:"
echo "  $PACKAGE_ROOT"
echo

mkdir -p native/src docs launchers

cp "$PACKAGE_ROOT/native/src/pluto_audio_monitor.c" native/src/pluto_audio_monitor.c
cp "$PACKAGE_ROOT/docs/AUDIO_MONITOR.md" docs/AUDIO_MONITOR.md
cp "$PACKAGE_ROOT/launchers/run_noaa_audio.cmd" launchers/run_noaa_audio.cmd

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
echo "Test:"
echo "  ./build/native/pluto_audio_monitor.exe --mode nfm --freq 162550000 --seconds 10 --wav noaa.wav"
