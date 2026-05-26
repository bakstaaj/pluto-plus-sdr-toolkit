# Pluto Dual RX Power Scanner

This package adds `pluto_dual_rx_power_scan`, a simple power scanner that can use RX1 only or both Pluto+ AD9361 RX channels.

## Intended baseline

Pluto+ firmware/env baseline:

```text
Firmware: ADI PlutoSDR v0.39
compatible=ad9361
mode=2r2t
```

Expected RX buffer mapping:

```text
RX1 = cf-ad9361-lpc voltage0 I + voltage1 Q
RX2 = cf-ad9361-lpc voltage2 I + voltage3 Q
```

## Options

```text
--rx-mode auto|single|dual
--rx-combine max|average|separate
```

Recommended defaults:

```text
--rx-mode auto
--rx-combine max
```

Meaning:

- `auto`: use dual RX when voltage0/1/2/3 are present; otherwise use RX1 only.
- `single`: force RX1 only.
- `dual`: require RX1 and RX2. Exit if RX2 is missing.
- `max`: combined power is the louder of RX1/RX2.
- `average`: combined power is the average of RX1/RX2 power.
- `separate`: log RX1 and RX2 separately; active decision still uses the louder receiver.

## Install

Extract to:

```text
C:\Users\jim\Downloads\PlutoDualRxPowerScanner
```

Run from MSYS2 UCRT64:

```bash
cd ~/sdrdev/pluto_native_test
/c/Users/jim/Downloads/PlutoDualRxPowerScanner/tools/install_dual_rx_power_scanner.sh
./tools/build_native_ucrt64.sh
```

If the installer was copied into repo/tools first:

```bash
./tools/install_dual_rx_power_scanner.sh /c/Users/jim/Downloads/PlutoDualRxPowerScanner
./tools/build_native_ucrt64.sh
```

## Verify build output

Use `./build`, not `/build`:

```bash
find ./build -iname 'pluto_dual_rx_power_scan.exe' -o -iname 'pluto_dual_rx_power_scan*'
```

Expected usual path:

```text
./build/native/pluto_dual_rx_power_scan.exe
```

## Host test

```bash
./build/native/pluto_dual_rx_power_scan.exe \
  --uri ip:192.168.2.1 \
  --freq-file configs/dual_rx_test_freqs.csv \
  --rx-mode auto \
  --rx-combine max \
  --threshold-dbfs -55 \
  --csv dual_rx_power_scan_host.csv \
  --verbose
```

Expected startup:

```text
RX1 channels: voltage0/voltage1 OK
RX2 channels: voltage2/voltage3 OK
Effective RX mode: dual
Dual available: yes
```

## Range test

```bash
./build/native/pluto_dual_rx_power_scan.exe \
  --uri ip:192.168.2.1 \
  --start 144000000 \
  --stop 148000000 \
  --step 25000 \
  --rx-mode auto \
  --rx-combine max \
  --threshold-dbfs -55 \
  --csv dual_rx_2m_range.csv
```

## Deploy Pluto-side launcher/config

```bash
./tools/deploy_pluto_dual_rx_power_scanner_msys2.sh
```

The deploy script uses `scp -O`.

## Pluto-side binary target

After an ARM build, copy the ARM executable to:

```text
/mnt/jffs2/pluto_ham_scan/bin/pluto_dual_rx_power_scan
```

Then run:

```bash
ssh root@192.168.2.1 "/mnt/jffs2/pluto_ham_scan/launchers/run_dual_rx_power_scan_storage.sh"
```

Expected output CSV:

```text
/tmp/pluto_ham_scan/sessions/dual_rx_power_scan.csv
```
