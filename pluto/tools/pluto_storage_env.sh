#!/bin/sh
APP_ROOT="${APP_ROOT:-/mnt/jffs2/pluto_ham_scan}"
if [ -f "$APP_ROOT/storage.env" ]; then . "$APP_ROOT/storage.env"; else STORAGE_BACKEND="${STORAGE_BACKEND:-tmpfs}"; DATA_ROOT="${DATA_ROOT:-/tmp/pluto_ham_scan}"; SESSION_DIR="$DATA_ROOT/sessions"; CAPTURE_DIR="$DATA_ROOT/captures"; UPLOAD_DIR="$DATA_ROOT/uploads"; DOWNLOAD_DIR="$DATA_ROOT/downloads"; LOG_DIR="$DATA_ROOT/logs"; REPORT_DIR="$DATA_ROOT/reports"; TMP_DIR="$DATA_ROOT/tmp"; CONFIG_DIR="$DATA_ROOT/config"; fi
export APP_ROOT STORAGE_BACKEND DATA_ROOT SESSION_DIR CAPTURE_DIR UPLOAD_DIR DOWNLOAD_DIR LOG_DIR REPORT_DIR TMP_DIR CONFIG_DIR
