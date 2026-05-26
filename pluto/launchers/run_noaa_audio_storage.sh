#!/bin/sh
set -eu
APP_ROOT="${APP_ROOT:-/mnt/jffs2/pluto_ham_scan}"; "$APP_ROOT/tools/pluto_storage_prepare.sh"; . "$APP_ROOT/tools/pluto_storage_env.sh"
AUDIO_EXE="${AUDIO_EXE:-$APP_ROOT/bin/pluto_audio_monitor}"; [ -x "$AUDIO_EXE" ]||{ echo "ERROR: missing $AUDIO_EXE" >&2; exit 1; }
mkdir -p "$SESSION_DIR"
exec "$AUDIO_EXE" --mode nfm --preset noaa7 --rate 960000 --audio-rate 48000 --seconds "${SECONDS:-30}" --squelch-db "${SQUELCH_DB:--65}" --wav "$SESSION_DIR/noaa.wav" --csv "$SESSION_DIR/audio_log.csv" "$@"
