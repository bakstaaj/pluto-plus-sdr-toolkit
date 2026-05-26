#!/usr/bin/env bash
set -Eeuo pipefail
PACKAGE_ROOT_WINDOWS="${PACKAGE_ROOT_WINDOWS:-C:\\Users\\jim\\Downloads\\PlutoDualRxTmpStorage}"
PACKAGE_ROOT="$(cygpath -u "$PACKAGE_ROOT_WINDOWS" 2>/dev/null || echo "$PACKAGE_ROOT_WINDOWS")"
[ -d "$PACKAGE_ROOT" ]||{ echo "ERROR: missing $PACKAGE_ROOT_WINDOWS" >&2; exit 1; }
mkdir -p native/src tools docs pluto/tools pluto/launchers
cp "$PACKAGE_ROOT/native/src/pluto_dual_rx_probe.c" native/src/
cp "$PACKAGE_ROOT/pluto/tools/"*.sh pluto/tools/
cp "$PACKAGE_ROOT/pluto/launchers/"*.sh pluto/launchers/
cp "$PACKAGE_ROOT/tools/deploy_pluto_dual_rx_tmp_storage_msys2.sh" tools/
cp "$PACKAGE_ROOT/docs/DUAL_RX_TMP_STORAGE.md" docs/
chmod +x tools/deploy_pluto_dual_rx_tmp_storage_msys2.sh pluto/tools/*.sh pluto/launchers/*.sh
if [ -f native/CMakeLists.txt ] && ! grep -q pluto_dual_rx_probe native/CMakeLists.txt; then cat >> native/CMakeLists.txt <<'EOF'

add_executable(pluto_dual_rx_probe src/pluto_dual_rx_probe.c)
target_link_libraries(pluto_dual_rx_probe PRIVATE PkgConfig::LIBIIO PkgConfig::LIBAD9361 m)

if (MINGW)
    target_compile_options(pluto_dual_rx_probe PRIVATE -Wall -Wextra -O2)
endif()
EOF
fi
echo "Installed. Build with ./tools/build_native_ucrt64.sh"
