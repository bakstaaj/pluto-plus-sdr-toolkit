#!/usr/bin/env bash
set -Eeuo pipefail

# install_dual_rx_probe_build_fix.sh
#
# Run from repo root:
#   cd ~/sdrdev/pluto_native_test
#   ./tools/install_dual_rx_probe_build_fix.sh
#
# Assumes package extracted to:
#   C:\Users\jim\Downloads\PlutoDualRxProbeBuildFix

PACKAGE_ROOT_WINDOWS="${PACKAGE_ROOT_WINDOWS:-C:\\Users\\jim\\Downloads\\PlutoDualRxProbeBuildFix}"

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

mkdir -p native/src docs

cp "$PACKAGE_ROOT/native/src/pluto_dual_rx_probe.c" native/src/pluto_dual_rx_probe.c
cp "$PACKAGE_ROOT/docs/DUAL_RX_PROBE_BUILD_FIX.md" docs/DUAL_RX_PROBE_BUILD_FIX.md

if [ -f native/CMakeLists.txt ] && ! grep -q "pluto_dual_rx_probe" native/CMakeLists.txt; then
    cat >> native/CMakeLists.txt <<'EOF'

add_executable(pluto_dual_rx_probe src/pluto_dual_rx_probe.c)
target_link_libraries(pluto_dual_rx_probe PRIVATE PkgConfig::LIBIIO PkgConfig::LIBAD9361 m)

if (MINGW)
    target_compile_options(pluto_dual_rx_probe PRIVATE -Wall -Wextra -O2)
endif()
EOF
    echo "Added pluto_dual_rx_probe target to native/CMakeLists.txt"
fi

echo
echo "Installed fixed native/src/pluto_dual_rx_probe.c"
echo
echo "Build:"
echo "  ./tools/build_native_ucrt64.sh"
