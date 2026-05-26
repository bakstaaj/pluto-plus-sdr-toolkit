#!/usr/bin/env bash
set -euo pipefail
cd "$(git rev-parse --show-toplevel 2>/dev/null || pwd)"

OUT="sweep_dual_rx_noaa_test.csv"
rm -f "$OUT"

echo "=== Short NOAA sweep with RX auto/max ==="
./build/native/pluto_sweep_scanner.exe \
  --uri ip:192.168.2.1 \
  --start 162400000 \
  --stop 162550000 \
  --step 25000 \
  --rate 1000000 \
  --bw 1000000 \
  --fft 4096 \
  --avg 2 \
  --top 3 \
  --threshold-db 8 \
  --dc-exclude-hz 5000 \
  --rx-mode auto \
  --rx-combine max \
  --csv "$OUT"

echo
echo "=== CSV preview ==="
head -20 "$OUT" 2>/dev/null || true
