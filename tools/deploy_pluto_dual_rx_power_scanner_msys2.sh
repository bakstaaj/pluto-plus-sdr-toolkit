#!/usr/bin/env bash
set -euo pipefail

# Deploy Pluto-side config and launcher for the Dual RX Power Scanner.
# Run from MSYS2 UCRT64 repo root:
#   cd ~/sdrdev/pluto_native_test
#   ./tools/deploy_pluto_dual_rx_power_scanner_msys2.sh
#
# This script intentionally uses scp -O for Pluto compatibility.

PLUTO_HOST="${PLUTO_HOST:-root@192.168.2.1}"
REMOTE_ROOT="${REMOTE_ROOT:-/mnt/jffs2/pluto_ham_scan}"

echo "Deploying to: $PLUTO_HOST:$REMOTE_ROOT"

ssh "$PLUTO_HOST" "mkdir -p '$REMOTE_ROOT/configs' '$REMOTE_ROOT/launchers' '$REMOTE_ROOT/bin' '$REMOTE_ROOT/tools'"

scp -O "configs/dual_rx_test_freqs.csv" \
  "$PLUTO_HOST:$REMOTE_ROOT/configs/dual_rx_test_freqs.csv"

scp -O "pluto/launchers/run_dual_rx_power_scan_storage.sh" \
  "$PLUTO_HOST:$REMOTE_ROOT/launchers/run_dual_rx_power_scan_storage.sh"

ssh "$PLUTO_HOST" "chmod +x '$REMOTE_ROOT/launchers/run_dual_rx_power_scan_storage.sh'"

echo
echo "Launcher/config deploy complete."
echo
echo "The ARM binary still needs to exist at:"
echo "  $REMOTE_ROOT/bin/pluto_dual_rx_power_scan"
echo
echo "After cross-compiling, copy it with:"
echo "  scp -O <arm-build-output>/pluto_dual_rx_power_scan \\"
echo "    $PLUTO_HOST:$REMOTE_ROOT/bin/pluto_dual_rx_power_scan"
echo "  ssh $PLUTO_HOST \"chmod +x $REMOTE_ROOT/bin/pluto_dual_rx_power_scan\""
echo
echo "Run on Pluto+:"
echo "  ssh $PLUTO_HOST \"$REMOTE_ROOT/launchers/run_dual_rx_power_scan_storage.sh\""
