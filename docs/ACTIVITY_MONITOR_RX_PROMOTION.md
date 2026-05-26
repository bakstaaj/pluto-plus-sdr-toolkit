# Activity Monitor Dual-RX Promotion

This package promotes the validated Windows-side dual-RX activity monitor logic into the production executable:

```text
build/native/pluto_activity_monitor.exe
```

It replaces:

```text
native/src/pluto_activity_monitor.c
```

with an implementation that supports:

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

- `auto`: use dual RX when RX1 and RX2 I/Q channels are available; otherwise fall back to RX1.
- `single`: force RX1 only.
- `dual`: require RX1 and RX2; fail if RX2 is missing.
- `max`: combined level is the louder of RX1/RX2.
- `average`: combined level is the average of RX1/RX2 power.
- `separate`: log RX1/RX2 separately while still using the louder value for activity decisions.

## Install

Extract to:

```text
C:\Users\jim\Downloads\PlutoActivityMonitorPromoteRx
```

Run from MSYS2 UCRT64:

```bash
cd ~/sdrdev/pluto_native_test
/c/Users/jim/Downloads/PlutoActivityMonitorPromoteRx/tools/install_activity_monitor_promote_rx.sh
./tools/build_native_ucrt64.sh
```

## Test

```bash
./tools/run_activity_monitor_production_rx_test_msys2.sh
```

Expected signs:

```text
RX1 channels: voltage0/voltage1 OK
RX2 channels: voltage2/voltage3 OK
Effective RX mode: dual
Dual available: yes
```

Expected CSV:

```text
activity_monitor_production_rx_test.csv
```

The installer makes a timestamped backup of the prior source file:

```text
native/src/pluto_activity_monitor.c.bak_pre_rx_promote_YYYYMMDD_HHMMSS
```

Do not commit generated CSV files or backup files.
