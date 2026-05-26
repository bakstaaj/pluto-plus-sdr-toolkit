#!/bin/sh
set -eu
APP_ROOT="${APP_ROOT:-/mnt/jffs2/pluto_ham_scan}"
REQ="${STORAGE_BACKEND_REQUEST:-auto}"
SD_MOUNT="${SD_MOUNT:-/mnt/sdcard}"; SD_DATA_ROOT="${SD_DATA_ROOT:-$SD_MOUNT/pluto_ham_scan}"
USB_MOUNT="${USB_MOUNT:-/mnt/usb}"; USB_DATA_ROOT="${USB_DATA_ROOT:-$USB_MOUNT/pluto_ham_scan}"
TMPFS_DATA_ROOT="${TMPFS_DATA_ROOT:-/tmp/pluto_ham_scan}"; JFFS2_DATA_ROOT="${JFFS2_DATA_ROOT:-$APP_ROOT/data}"
MIGRATE=0; LINKS=1
for a in "$@"; do case "$a" in --migrate)MIGRATE=1;; --no-links)LINKS=0;; --backend=*)REQ="${a#--backend=}";; --help|-h)echo "Usage: $0 [--migrate] [--backend=auto|sdcard|usb|tmpfs|jffs2]";exit 0;; *)echo "bad option $a" >&2;exit 1;; esac; done
log(){ echo "[pluto-storage] $*"; }; warn(){ echo "[pluto-storage] WARNING: $*" >&2; }; die(){ echo "[pluto-storage] ERROR: $*" >&2; exit 1; }
ism(){ grep -q " $1 " /proc/mounts 2>/dev/null; }
wr(){ mkdir -p "$1"; t="$1/.write_test_$$"; echo test > "$t" 2>/dev/null || return 1; rm -f "$t"; return 0; }
fsd(){ for d in /dev/mmcblk0p1 /dev/mmcblk1p1 /dev/mmcblk0 /dev/mmcblk1; do [ -b "$d" ]&&{ echo "$d";return 0;}; done; return 1; }
fud(){ for d in /dev/sda1 /dev/sdb1 /dev/sdc1 /dev/sda /dev/sdb /dev/sdc; do [ -b "$d" ]&&{ echo "$d";return 0;}; done; return 1; }
mt(){ mkdir -p "$2"; ism "$2"&&return 0; mount -o rw,noatime "$1" "$2" 2>/tmp/pluto_storage_mount.err&&return 0; for fs in vfat ext4 ext3 ext2; do mount -t "$fs" -o rw,noatime "$1" "$2" 2>/tmp/pluto_storage_mount.err&&return 0; done; return 1; }
sel_sd(){ ism "$SD_MOUNT"&&wr "$SD_DATA_ROOT"&&{ BACKEND=sdcard;DATA_ROOT="$SD_DATA_ROOT";return 0;}; d="$(fsd||true)"; [ -n "$d" ]&&mt "$d" "$SD_MOUNT"&&wr "$SD_DATA_ROOT"&&{ BACKEND=sdcard;DATA_ROOT="$SD_DATA_ROOT";return 0;}; return 1; }
sel_usb(){ ism "$USB_MOUNT"&&wr "$USB_DATA_ROOT"&&{ BACKEND=usb;DATA_ROOT="$USB_DATA_ROOT";return 0;}; d="$(fud||true)"; [ -n "$d" ]&&mt "$d" "$USB_MOUNT"&&wr "$USB_DATA_ROOT"&&{ BACKEND=usb;DATA_ROOT="$USB_DATA_ROOT";return 0;}; return 1; }
sel_tmp(){ BACKEND=tmpfs; DATA_ROOT="$TMPFS_DATA_ROOT"; wr "$DATA_ROOT"; }
sel_jffs(){ BACKEND=jffs2; DATA_ROOT="$JFFS2_DATA_ROOT"; wr "$DATA_ROOT"||die "JFFS2 not writable"; }
case "$REQ" in auto) sel_sd||{ warn "SD-card backend not available; falling back."; sel_usb||{ warn "USB backend not available; falling back."; sel_tmp||{ warn "tmpfs not available; falling back."; sel_jffs;};};};; sdcard)sel_sd||die "sdcard requested but unavailable";; usb)sel_usb||die "usb requested but unavailable";; tmpfs)sel_tmp||die "tmpfs unavailable";; jffs2)sel_jffs;; *)die "bad backend $REQ";; esac
SESSION_DIR="$DATA_ROOT/sessions"; CAPTURE_DIR="$DATA_ROOT/captures"; UPLOAD_DIR="$DATA_ROOT/uploads"; DOWNLOAD_DIR="$DATA_ROOT/downloads"; LOG_DIR="$DATA_ROOT/logs"; REPORT_DIR="$DATA_ROOT/reports"; TMP_DIR="$DATA_ROOT/tmp"; CONFIG_DIR="$DATA_ROOT/config"
mkdir -p "$SESSION_DIR" "$CAPTURE_DIR" "$UPLOAD_DIR" "$DOWNLOAD_DIR" "$LOG_DIR" "$REPORT_DIR" "$TMP_DIR" "$CONFIG_DIR" "$APP_ROOT"
linkone(){ [ "$LINKS" -eq 1 ]||return 0; n="$1"; tgt="$2"; l="$APP_ROOT/$n"; mkdir -p "$tgt"; if [ -L "$l" ]; then cur="$(readlink "$l")"; [ "$cur" = "$tgt" ]&&{ log "Link OK: $l -> $tgt"; return 0;}; rm -f "$l"; ln -s "$tgt" "$l"; return 0; fi; if [ -e "$l" ]; then if [ "$MIGRATE" -eq 1 ]; then cp -a "$l/." "$tgt/" 2>/dev/null||cp -r "$l/." "$tgt/"; mv "$l" "$l.jffs2.backup.$(date +%Y%m%d_%H%M%S)"; ln -s "$tgt" "$l"; else warn "$l exists, not symlink; use --migrate if desired"; fi; return 0; fi; ln -s "$tgt" "$l"; log "Created link: $l -> $tgt"; }
linkone data "$DATA_ROOT"; linkone sessions "$SESSION_DIR"; linkone captures "$CAPTURE_DIR"; linkone uploads "$UPLOAD_DIR"; linkone downloads "$DOWNLOAD_DIR"; linkone logs "$LOG_DIR"; linkone reports "$REPORT_DIR"; linkone tmp "$TMP_DIR"
cat > "$APP_ROOT/storage.env" <<EOF
APP_ROOT="$APP_ROOT"
STORAGE_BACKEND="$BACKEND"
STORAGE_BACKEND_REQUEST="$REQ"
SD_MOUNT="$SD_MOUNT"
SD_DATA_ROOT="$SD_DATA_ROOT"
USB_MOUNT="$USB_MOUNT"
USB_DATA_ROOT="$USB_DATA_ROOT"
TMPFS_DATA_ROOT="$TMPFS_DATA_ROOT"
JFFS2_DATA_ROOT="$JFFS2_DATA_ROOT"
DATA_ROOT="$DATA_ROOT"
SESSION_DIR="$SESSION_DIR"
CAPTURE_DIR="$CAPTURE_DIR"
UPLOAD_DIR="$UPLOAD_DIR"
DOWNLOAD_DIR="$DOWNLOAD_DIR"
LOG_DIR="$LOG_DIR"
REPORT_DIR="$REPORT_DIR"
TMP_DIR="$TMP_DIR"
CONFIG_DIR="$CONFIG_DIR"
EOF
log "Selected backend: $BACKEND"; log "DATA_ROOT=$DATA_ROOT"; log "SESSION_DIR=$SESSION_DIR"; [ "$BACKEND" = tmpfs ]&&warn "Using volatile /tmp storage. Files disappear after reboot."; df -h "$APP_ROOT" "$DATA_ROOT" /tmp 2>/dev/null||true
