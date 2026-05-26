#!/usr/bin/env bash
set -euo pipefail
cd "$(git rev-parse --show-toplevel 2>/dev/null || pwd)"

echo "=== Verify help contains RX options ==="
./build/native/pluto_sweep_scanner.exe --help | grep -E -- '--rx-mode|--rx-combine'
./build/native/pluto_band_scan.exe --help | grep -E -- '--rx-mode|--rx-combine'
./build/native/pluto_scan_session.exe --help | grep -E -- '--rx-mode|--rx-combine'

echo
echo "=== Band scan dry run ==="
./build/native/pluto_band_scan.exe \
  --band noaa \
  --uri ip:192.168.2.1 \
  --rx-mode auto \
  --rx-combine max \
  --out-prefix noaa_rx_dryrun \
  --dry-run

echo
echo "=== Scan session dry run ==="
./build/native/pluto_scan_session.exe \
  --band noaa \
  --uri ip:192.168.2.1 \
  --cycles 1 \
  --rx-mode auto \
  --rx-combine max \
  --out-prefix noaa_session_rx_dryrun \
  --no-report \
  --dry-run

echo
echo "Dry-run checks complete."
