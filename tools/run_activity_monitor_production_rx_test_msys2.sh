#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

EXE="./build/native/pluto_activity_monitor.exe"
if [[ ! -x "$EXE" ]]; then
  echo "ERROR: $EXE not found or not executable. Run ./tools/build_native_ucrt64.sh first." >&2
  exit 1
fi

"$EXE" \
  --uri ip:192.168.2.1 \
  --freq-file configs/activity_rx_test_freqs.csv \
  --rx-mode auto \
  --rx-combine max \
  --threshold-dbfs -55 \
  --csv activity_monitor_production_rx_test.csv \
  --verbose

echo
echo "CSV preview:"
head -20 activity_monitor_production_rx_test.csv 2>/dev/null || true
