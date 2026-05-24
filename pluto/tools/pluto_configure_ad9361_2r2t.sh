#!/bin/sh
# pluto_configure_ad9361_2r2t.sh
#
# Configure Pluto+/Rev.C U-Boot environment for:
#   compatible=ad9361
#   mode=2r2t
#
# This follows the ADI Pluto customization flow:
#
#   fw_setenv attr_name compatible
#   fw_setenv attr_val ad9361
#   fw_setenv compatible ad9361
#   fw_setenv mode 2r2t
#   reboot
#
# Default behavior:
#   - Shows current config
#   - Requires --yes to make changes
#   - Does NOT reboot unless --reboot is also passed
#
# Run on Pluto+:
#   /mnt/jffs2/pluto_ham_scan/tools/pluto_configure_ad9361_2r2t.sh --yes
#   /mnt/jffs2/pluto_ham_scan/tools/pluto_configure_ad9361_2r2t.sh --yes --reboot

set -eu

DO_WRITE=0
DO_REBOOT=0

for arg in "$@"; do
    case "$arg" in
        --yes)
            DO_WRITE=1
            ;;
        --reboot)
            DO_REBOOT=1
            ;;
        --help|-h)
            cat <<EOF
Usage:
  $0 --yes [--reboot]

Options:
  --yes      Actually write fw_setenv values.
  --reboot   Reboot after writing values.

This configures:
  attr_name=compatible
  attr_val=ad9361
  compatible=ad9361
  mode=2r2t
EOF
            exit 0
            ;;
        *)
            echo "ERROR: Unknown option: $arg" >&2
            exit 1
            ;;
    esac
done

say() {
    echo "[pluto-2r2t-config] $*"
}

show_env() {
    fw_printenv attr_name 2>/dev/null || true
    fw_printenv attr_val 2>/dev/null || true
    fw_printenv compatible 2>/dev/null || true
    fw_printenv mode 2>/dev/null || true
}

if ! command -v fw_setenv >/dev/null 2>&1; then
    echo "ERROR: fw_setenv not found." >&2
    exit 1
fi

if ! command -v fw_printenv >/dev/null 2>&1; then
    echo "ERROR: fw_printenv not found." >&2
    exit 1
fi

say "Current environment:"
show_env
echo

say "Target environment:"
echo "attr_name=compatible"
echo "attr_val=ad9361"
echo "compatible=ad9361"
echo "mode=2r2t"
echo

if [ "$DO_WRITE" -ne 1 ]; then
    say "Dry run only. No changes made."
    say "Run with --yes to apply."
    exit 0
fi

say "Writing U-Boot environment..."
fw_setenv attr_name compatible
fw_setenv attr_val ad9361
fw_setenv compatible ad9361
fw_setenv mode 2r2t

echo
say "Updated environment:"
show_env
echo

if [ "$DO_REBOOT" -eq 1 ]; then
    say "Rebooting now..."
    reboot
else
    say "Reboot required for changes to fully apply."
    say "Run: reboot"
fi
