#!/usr/bin/env bash
set -Eeuo pipefail
PLUTO_HOST="${PLUTO_HOST:-192.168.2.1}"; PLUTO_USER="${PLUTO_USER:-root}"; REMOTE_ROOT="${REMOTE_ROOT:-/mnt/jffs2/pluto_ham_scan}"
if [ -d pluto/tools ]; then SRC_ROOT="."; else SRC_ROOT="/c/Users/jim/Downloads/PlutoDualRxTmpStorage"; fi
ssh "$PLUTO_USER@$PLUTO_HOST" "mkdir -p $REMOTE_ROOT/tools $REMOTE_ROOT/launchers $REMOTE_ROOT/bin"
scp -O "$SRC_ROOT"/pluto/tools/*.sh "$PLUTO_USER@$PLUTO_HOST:$REMOTE_ROOT/tools/"
scp -O "$SRC_ROOT"/pluto/launchers/*.sh "$PLUTO_USER@$PLUTO_HOST:$REMOTE_ROOT/launchers/"
ssh "$PLUTO_USER@$PLUTO_HOST" "chmod +x $REMOTE_ROOT/tools/*.sh $REMOTE_ROOT/launchers/*.sh && $REMOTE_ROOT/tools/pluto_storage_prepare.sh && $REMOTE_ROOT/tools/pluto_storage_status.sh"
echo "Deploy complete. Copy ARM pluto_dual_rx_probe to $REMOTE_ROOT/bin/pluto_dual_rx_probe"
