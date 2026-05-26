#!/bin/sh
set -eu
APP_ROOT="${APP_ROOT:-/mnt/jffs2/pluto_ham_scan}"
"$APP_ROOT/tools/pluto_storage_prepare.sh"; . "$APP_ROOT/tools/pluto_storage_env.sh"
PROBE_EXE="${PROBE_EXE:-$APP_ROOT/bin/pluto_dual_rx_probe}"; [ -x "$PROBE_EXE" ]||{ echo "ERROR: missing $PROBE_EXE" >&2; exit 1; }
mkdir -p "$SESSION_DIR"
exec "$PROBE_EXE" --uri "${IIO_URI:-ip:localhost}" --freq "${FREQ_HZ:-146520000}" --rate "${RATE_HZ:-960000}" --bw "${BW_HZ:-1000000}" --rx-mode "${RX_MODE:-auto}" --seconds "${SECONDS:-2}" --csv "$SESSION_DIR/dual_rx_probe.csv" "$@"
