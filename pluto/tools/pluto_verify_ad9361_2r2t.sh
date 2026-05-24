#!/bin/sh
# pluto_verify_ad9361_2r2t.sh
#
# Verify Pluto+/Rev.C environment is configured for:
#   compatible=ad9361
#   mode=2r2t
#
# Expected result:
#   70 MHz range support through AD9361-compatible mode
#   2 RX / 2 TX IIO channel exposure
#
# Run on Pluto+:
#   /mnt/jffs2/pluto_ham_scan/tools/pluto_verify_ad9361_2r2t.sh

set -eu

IIO_URI="${IIO_URI:-ip:localhost}"
TMP_IIO="/tmp/pluto_iio_verify_2r2t_$$.txt"
PASS=1

say() {
    echo "[pluto-2r2t] $*"
}

fail() {
    echo "[pluto-2r2t] FAIL: $*" >&2
    PASS=0
}

check_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        fail "missing command: $1"
        return 1
    fi
    return 0
}

get_fw_env() {
    name="$1"
    fw_printenv "$name" 2>/dev/null | sed "s/^$name=//"
}

check_env_value() {
    name="$1"
    expected="$2"
    actual="$(get_fw_env "$name" || true)"

    if [ "$actual" = "$expected" ]; then
        say "OK: $name=$actual"
    else
        fail "expected $name=$expected, got '${actual:-missing}'"
    fi
}

say "Verifying Pluto+ AD9361 / 2R2T configuration"
say "IIO_URI=$IIO_URI"
echo

check_cmd fw_printenv || true
check_cmd iio_info || true

echo "Firmware environment:"
fw_printenv attr_name 2>/dev/null || true
fw_printenv attr_val 2>/dev/null || true
fw_printenv compatible 2>/dev/null || true
fw_printenv mode 2>/dev/null || true
echo

check_env_value attr_name compatible
check_env_value attr_val ad9361
check_env_value compatible ad9361
check_env_value mode 2r2t

echo
say "Reading IIO context..."
if ! iio_info -u "$IIO_URI" > "$TMP_IIO" 2>/tmp/pluto_iio_verify_2r2t.err; then
    cat /tmp/pluto_iio_verify_2r2t.err >&2 || true
    rm -f "$TMP_IIO"
    fail "iio_info failed for $IIO_URI"
else
    model_line="$(grep -m1 'ad9361-phy,model:' "$TMP_IIO" || true)"
    echo "$model_line"

    if echo "$model_line" | grep -q 'ad9361-phy,model: ad9361'; then
        say "OK: IIO model reports ad9361"
    else
        fail "IIO model is not ad9361"
    fi

    rx_block="$(sed -n '/cf-ad9361-lpc/,/No trigger/p' "$TMP_IIO" || true)"

    echo
    echo "RX buffer channels:"
    echo "$rx_block" | grep -E 'voltage[0-3]:.*input.*index' || true
    echo

    for ch in 0 1 2 3; do
        if echo "$rx_block" | grep -q "voltage${ch}:.*input.*index: ${ch}"; then
            say "OK: RX buffer voltage${ch} input index ${ch}"
        else
            fail "missing RX buffer voltage${ch} input index ${ch}"
        fi
    done

    tx_block="$(sed -n '/cf-ad9361-dds-core-lpc/,/cf-ad9361-lpc/p' "$TMP_IIO" || true)"

    for ch in 0 1 2 3; do
        if echo "$tx_block" | grep -q "voltage${ch}:.*output.*index: ${ch}"; then
            say "OK: TX buffer voltage${ch} output index ${ch}"
        else
            fail "missing TX buffer voltage${ch} output index ${ch}"
        fi
    done

    rm -f "$TMP_IIO"
fi

echo

if [ "$PASS" -eq 1 ]; then
    say "PASS: Pluto+ is configured as AD9361 / 2R2T."
    exit 0
fi

say "FAILED: Pluto+ is not fully configured as AD9361 / 2R2T."
exit 1
