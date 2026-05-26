#!/usr/bin/env bash
set -euo pipefail

# Install PlutoDualRxPowerScanner files into the repo layout.
# Run from repo root:
#   cd ~/sdrdev/pluto_native_test
#   ./tools/install_dual_rx_power_scanner.sh

ROOT="$(pwd)"
PKG_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "Repo root: $ROOT"
echo "Package dir: $PKG_DIR"

mkdir -p "$ROOT/native/src" "$ROOT/tools" "$ROOT/pluto/launchers" "$ROOT/configs" "$ROOT/docs"

copy_file() {
  local src="$1"
  local dst="$2"
  echo "Installing $dst"
  cp "$src" "$dst"
}

copy_file "$PKG_DIR/native/src/pluto_dual_rx_power_scan.c" "$ROOT/native/src/pluto_dual_rx_power_scan.c"
copy_file "$PKG_DIR/tools/deploy_pluto_dual_rx_power_scanner_msys2.sh" "$ROOT/tools/deploy_pluto_dual_rx_power_scanner_msys2.sh"
copy_file "$PKG_DIR/pluto/launchers/run_dual_rx_power_scan_storage.sh" "$ROOT/pluto/launchers/run_dual_rx_power_scan_storage.sh"
copy_file "$PKG_DIR/configs/dual_rx_test_freqs.csv" "$ROOT/configs/dual_rx_test_freqs.csv"
copy_file "$PKG_DIR/docs/DUAL_RX_POWER_SCANNER.md" "$ROOT/docs/DUAL_RX_POWER_SCANNER.md"

chmod +x "$ROOT/tools/deploy_pluto_dual_rx_power_scanner_msys2.sh"
chmod +x "$ROOT/pluto/launchers/run_dual_rx_power_scan_storage.sh"

CMAKE_FILE="$ROOT/native/CMakeLists.txt"
if [[ ! -f "$CMAKE_FILE" ]]; then
  echo "ERROR: $CMAKE_FILE not found."
  echo "Install files were copied, but CMake was not updated."
  exit 1
fi

if grep -q "BEGIN PlutoDualRxPowerScanner" "$CMAKE_FILE"; then
  echo "CMake block already present; not adding duplicate target."
else
  cat >> "$CMAKE_FILE" <<'EOF'

# BEGIN PlutoDualRxPowerScanner
if(NOT TARGET pluto_dual_rx_power_scan)
  add_executable(pluto_dual_rx_power_scan src/pluto_dual_rx_power_scan.c)

  if(TARGET iio)
    target_link_libraries(pluto_dual_rx_power_scan PRIVATE iio)
  elseif(DEFINED IIO_LIBRARIES)
    target_link_libraries(pluto_dual_rx_power_scan PRIVATE ${IIO_LIBRARIES})
  elseif(DEFINED LIBIIO_LIBRARIES)
    target_link_libraries(pluto_dual_rx_power_scan PRIVATE ${LIBIIO_LIBRARIES})
  else()
    target_link_libraries(pluto_dual_rx_power_scan PRIVATE iio)
  endif()

  target_link_libraries(pluto_dual_rx_power_scan PRIVATE m)
endif()
# END PlutoDualRxPowerScanner
EOF
  echo "Added pluto_dual_rx_power_scan target to native/CMakeLists.txt"
fi

echo
echo "Install complete."
echo
echo "Next:"
echo "  ./tools/build_native_ucrt64.sh"
echo
echo "Host test:"
echo "  ./build/native/pluto_dual_rx_power_scan.exe \\"
echo "    --uri ip:192.168.2.1 \\"
echo "    --freq-file configs/dual_rx_test_freqs.csv \\"
echo "    --rx-mode auto \\"
echo "    --rx-combine max \\"
echo "    --threshold-dbfs -55 \\"
echo "    --csv dual_rx_power_scan_host.csv"
