#!/usr/bin/env bash
set -Eeuo pipefail

# deploy_pluto_2r2t_tools_msys2.sh
#
# Deploy AD9361/2R2T verify/config tools to Pluto+ using scp -O.
#
# Run from MSYS2 UCRT64:
#   cd ~/sdrdev/pluto_native_test
#   ./tools/deploy_pluto_2r2t_tools_msys2.sh
#
# Or directly from extracted package:
#   bash /c/Users/jim/Downloads/PlutoPlusAd9361_2R2T_Tools/tools/deploy_pluto_2r2t_tools_msys2.sh

PLUTO_HOST="${PLUTO_HOST:-192.168.2.1}"
PLUTO_USER="${PLUTO_USER:-root}"
REMOTE_ROOT="${REMOTE_ROOT:-/mnt/jffs2/pluto_ham_scan}"

if [ -d "pluto/tools" ] && [ -f "pluto/tools/pluto_verify_ad9361_2r2t.sh" ]; then
    SRC_ROOT="."
elif [ -d "/c/Users/jim/Downloads/PlutoPlusAd9361_2R2T_Tools/pluto/tools" ]; then
    SRC_ROOT="/c/Users/jim/Downloads/PlutoPlusAd9361_2R2T_Tools"
else
    echo "ERROR: Could not find pluto/tools source folder." >&2
    echo "Expected package at:" >&2
    echo "  C:\\Users\\jim\\Downloads\\PlutoPlusAd9361_2R2T_Tools" >&2
    exit 1
fi

echo "Deploying Pluto+ 2R2T tools"
echo "  Source: $SRC_ROOT"
echo "  Target: $PLUTO_USER@$PLUTO_HOST:$REMOTE_ROOT/tools"
echo

ssh "$PLUTO_USER@$PLUTO_HOST" "mkdir -p $REMOTE_ROOT/tools"

scp -O "$SRC_ROOT"/pluto/tools/pluto_verify_ad9361_2r2t.sh "$PLUTO_USER@$PLUTO_HOST:$REMOTE_ROOT/tools/"
scp -O "$SRC_ROOT"/pluto/tools/pluto_configure_ad9361_2r2t.sh "$PLUTO_USER@$PLUTO_HOST:$REMOTE_ROOT/tools/"

ssh "$PLUTO_USER@$PLUTO_HOST" "chmod +x $REMOTE_ROOT/tools/pluto_verify_ad9361_2r2t.sh $REMOTE_ROOT/tools/pluto_configure_ad9361_2r2t.sh"

echo
echo "Running remote verify:"
ssh "$PLUTO_USER@$PLUTO_HOST" "$REMOTE_ROOT/tools/pluto_verify_ad9361_2r2t.sh" || {
    echo
    echo "Remote verify failed."
    echo "To configure, run on Pluto+ only after confirming this is intended:"
    echo "  $REMOTE_ROOT/tools/pluto_configure_ad9361_2r2t.sh --yes"
    echo "  reboot"
    exit 1
}

echo
echo "Deploy and verify complete."
