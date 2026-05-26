#!/bin/sh
set -eu
APP_ROOT="${APP_ROOT:-/mnt/jffs2/pluto_ham_scan}"; "$APP_ROOT/tools/pluto_storage_prepare.sh"; . "$APP_ROOT/tools/pluto_storage_env.sh"
REPORT_EXE="${REPORT_EXE:-$APP_ROOT/bin/pluto_audio_report}"; [ -x "$REPORT_EXE" ]||{ echo "ERROR: missing $REPORT_EXE" >&2; exit 1; }; [ -f "$SESSION_DIR/audio_log.csv" ]||{ echo "ERROR: missing $SESSION_DIR/audio_log.csv" >&2; exit 1; }
exec "$REPORT_EXE" --in "$SESSION_DIR/audio_log.csv" --out "$SESSION_DIR/audio_report.html" "$@"
