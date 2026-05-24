#!/usr/bin/env bash
set -Eeuo pipefail

# install_pluto_2r2t_tools.sh
#
# Install Pluto+ AD9361/2R2T helper tools into the repo.
#
# Run from MSYS2 UCRT64:
#   cd ~/sdrdev/pluto_native_test
#   ./tools/install_pluto_2r2t_tools.sh
#
# Assumes package extracted to:
#   C:\Users\jim\Downloads\PlutoPlusAd9361_2R2T_Tools

PACKAGE_ROOT_WINDOWS="${PACKAGE_ROOT_WINDOWS:-C:\\Users\\jim\\Downloads\\PlutoPlusAd9361_2R2T_Tools}"

if command -v cygpath >/dev/null 2>&1; then
    PACKAGE_ROOT="$(cygpath -u "$PACKAGE_ROOT_WINDOWS")"
else
    PACKAGE_ROOT="$PACKAGE_ROOT_WINDOWS"
fi

if [ ! -d "$PACKAGE_ROOT" ]; then
    echo "ERROR: Package folder not found:"
    echo "  $PACKAGE_ROOT_WINDOWS"
    exit 1
fi

mkdir -p pluto/tools tools docs

cp "$PACKAGE_ROOT/pluto/tools/pluto_verify_ad9361_2r2t.sh" pluto/tools/
cp "$PACKAGE_ROOT/pluto/tools/pluto_configure_ad9361_2r2t.sh" pluto/tools/
cp "$PACKAGE_ROOT/tools/deploy_pluto_2r2t_tools_msys2.sh" tools/
cp "$PACKAGE_ROOT/docs/PLUTO_PLUS_AD9361_2R2T.md" docs/

chmod +x pluto/tools/pluto_verify_ad9361_2r2t.sh
chmod +x pluto/tools/pluto_configure_ad9361_2r2t.sh
chmod +x tools/deploy_pluto_2r2t_tools_msys2.sh

echo "Installed Pluto+ AD9361/2R2T tools."
echo
echo "Deploy and verify:"
echo "  ./tools/deploy_pluto_2r2t_tools_msys2.sh"
