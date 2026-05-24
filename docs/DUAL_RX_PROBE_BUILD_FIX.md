# Dual RX Probe Build Fix

This fixes the MSYS2 UCRT64 build error:

```text
implicit declaration of function 'localtime_r'; did you mean 'localtime_s'?
```

## Cause

`localtime_r()` is POSIX. MSYS2 UCRT64 targets Windows behavior and needs `localtime_s()`.

## Fix

`pluto_dual_rx_probe.c` now uses:

```c
#if defined(_WIN32)
    localtime_s(tm_value, t);
#else
    localtime_r(t, tm_value);
#endif
```

It also removes the misleading one-line `if` pattern that caused:

```text
-Wmisleading-indentation
```

## Install

```bash
cd ~/sdrdev/pluto_native_test

cp /c/Users/jim/Downloads/PlutoDualRxProbeBuildFix/tools/install_dual_rx_probe_build_fix.sh tools/
chmod +x tools/install_dual_rx_probe_build_fix.sh

./tools/install_dual_rx_probe_build_fix.sh
```

## Rebuild

```bash
./tools/build_native_ucrt64.sh
```
