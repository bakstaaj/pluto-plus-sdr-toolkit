#!/bin/sh
set -eu

APP_ROOT="${APP_ROOT:-/mnt/jffs2/pluto_ham_scan}"
BIN="$APP_ROOT/bin/pluto_dual_rx_power_scan"
FREQ_FILE="${FREQ_FILE:-$APP_ROOT/configs/dual_rx_test_freqs.csv}"

# Prefer the project's storage abstraction if present.
if [ -x "$APP_ROOT/tools/pluto_storage_prepare.sh" ]; then
  "$APP_ROOT/tools/pluto_storage_prepare.sh" >/dev/null 2>&1 || true
fi

if [ -f "$APP_ROOT/tools/pluto_storage_env.sh" ]; then
  # shellcheck disable=SC1090
  . "$APP_ROOT/tools/pluto_storage_env.sh"
fi

DATA_ROOT="${DATA_ROOT:-/tmp/pluto_ham_scan}"
SESSION_DIR="${SESSION_DIR:-$DATA_ROOT/sessions}"
mkdir -p "$SESSION_DIR"

CSV_OUT="${CSV_OUT:-$SESSION_DIR/dual_rx_power_scan.csv}"

if [ ! -x "$BIN" ]; then
  echo "ERROR: scanner binary not found or not executable:"
  echo "  $BIN"
  echo
  echo "Copy ARM binary to Pluto+ with:"
  echo "  scp -O <arm-build-output>/pluto_dual_rx_power_scan root@192.168.2.1:$BIN"
  echo "  ssh root@192.168.2.1 \"chmod +x $BIN\""
  exit 1
fi

if [ ! -f "$FREQ_FILE" ]; then
  echo "ERROR: frequency file not found:"
  echo "  $FREQ_FILE"
  exit 1
fi

echo "Pluto Dual RX Power Scanner"
echo "APP_ROOT=$APP_ROOT"
echo "STORAGE_BACKEND=${STORAGE_BACKEND:-unknown}"
echo "SESSION_DIR=$SESSION_DIR"
echo "CSV_OUT=$CSV_OUT"
echo "FREQ_FILE=$FREQ_FILE"
echo

exec "$BIN" \
  --uri local: \
  --freq-file "$FREQ_FILE" \
  --rx-mode "${RX_MODE:-auto}" \
  --rx-combine "${RX_COMBINE:-max}" \
  --threshold-dbfs "${THRESHOLD_DBFS:--55}" \
  --csv "$CSV_OUT" \
  ${EXTRA_ARGS:-}
