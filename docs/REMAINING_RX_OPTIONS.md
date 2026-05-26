# Remaining Windows RX Options Integration

This package updates the remaining Windows-side scanner workflow pieces:

- `native/src/pluto_sweep_scanner.c`
- `native/src/pluto_band_scan.c`
- `native/src/pluto_scan_session.c`

It keeps the project scoped to Windows/MSYS2 host applications that connect to the Pluto+ with `--uri ip:192.168.2.1`.

## New options

All three updated tools now accept:

```text
--rx-mode auto|single|dual
--rx-combine max|average|separate
```

Defaults:

```text
--rx-mode auto
--rx-combine max
```

## Behavior

- `auto` uses RX1 and RX2 when `cf-ad9361-lpc` exposes `voltage0/1/2/3`; otherwise it falls back to RX1.
- `single` uses RX1 only.
- `dual` requires RX2 and fails if `voltage2/3` are not present.
- `max` combines dual-RX sweep bins by selecting the stronger receiver bin.
- `average` combines dual-RX sweep bins by averaging linear power.
- `separate` is accepted for workflow consistency. For `pluto_sweep_scanner.exe`, CSV output remains compatible with `pluto_scan_group.exe`, so `separate` uses the same max-style peak selection while printing a note.

## Important compatibility note

The current promoted `pluto_activity_monitor.exe` uses the newer frequency-file style interface:

```text
--freq-file <csv>
--threshold-dbfs <db>
--rx-mode <mode>
--rx-combine <mode>
```

Therefore this package updates `pluto_scan_session.exe` to call the activity monitor with `--freq-file <prefix>_grouped.csv` instead of the older `--in/--cycles/--passband-hz/--avg` interface.

## Install

From MSYS2 UCRT64:

```bash
cd ~/sdrdev/pluto_native_test
/c/Users/jim/Downloads/PlutoRemainingRxOptions/tools/install_remaining_rx_options.sh
./tools/build_native_ucrt64.sh
```

## Test

```bash
./tools/run_remaining_rx_dryrun_msys2.sh
./tools/run_sweep_dual_rx_test_msys2.sh
```

## Commit

Do not commit generated CSV files or backup files.

```bash
git add \
  native/src/pluto_sweep_scanner.c \
  native/src/pluto_band_scan.c \
  native/src/pluto_scan_session.c \
  docs/REMAINING_RX_OPTIONS.md \
  tools/run_remaining_rx_dryrun_msys2.sh \
  tools/run_sweep_dual_rx_test_msys2.sh \
  launchers/run_noaa_session_dual_rx.cmd

git commit -m "Add dual RX options to remaining Windows scanners"
git push
```
