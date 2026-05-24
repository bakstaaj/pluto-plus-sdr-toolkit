# Release Test Checklist

Use this checklist from the repository root before publishing a Pluto+ SDR Windows Toolkit release.

Hardware assumption: this toolkit assumes a Pluto+ with RX coverage down to at least 70 MHz. FM broadcast scanning remains a standard test target.

## 1. Clean Git Status

```bash
git status --short
```

Expected result: no output.

## 2. Native Build

Run from MSYS2 UCRT64:

```bash
./tools/build_native_ucrt64.sh
```

Confirm key native tools exist:

```bash
test -f ./build/native/pluto_scan_session.exe
test -f ./build/native/pluto_spectrum_stream.exe
```

## 3. FM Dry Run

```bash
./build/native/pluto_scan_session.exe --config configs/fm.conf --dry-run
```

Expected result: command exits successfully without creating a real SDR capture session.

## 4. 2m Dry Run

```bash
./build/native/pluto_scan_session.exe --config configs/2m.conf --dry-run
```

Expected result: command exits successfully without creating a real SDR capture session.

## 5. NOAA Scan Session

Run a real NOAA session:

```bash
./build/native/pluto_scan_session.exe --config configs/noaa.conf
```

Confirm expected output files:

```bash
test -f noaa_raw.csv
test -f noaa_summary.csv
test -f noaa_report.html
```

## 6. Live Spectrum Streamer

Run a short live spectrum check:

```bash
./build/native/pluto_spectrum_stream.exe --freq 100000000 --rate 2000000 --fft 2048 --avg 2
```

Expected result: streamer starts, prints status or spectrum output, and can be stopped with `Ctrl+C`.

## 7. Release Packaging

Set a release version and package it:

```bash
VERSION=v0.4-test
./tools/package_windows_release.sh "$VERSION"
```

Expected release folder:

```bash
test -d "releases/pluto-plus-sdr-toolkit-${VERSION}"
```

## 8. Release Launcher Folder Contents

```bash
find "releases/pluto-plus-sdr-toolkit-${VERSION}/launchers" -maxdepth 1 -type f -name "*.cmd" -printf "%f\n" | sort
```

Required launchers:

```bash
test -f "releases/pluto-plus-sdr-toolkit-${VERSION}/launchers/run_fm_scan.cmd"
test -f "releases/pluto-plus-sdr-toolkit-${VERSION}/launchers/run_2m_scan.cmd"
test -f "releases/pluto-plus-sdr-toolkit-${VERSION}/launchers/run_noaa_scan.cmd"
test -f "releases/pluto-plus-sdr-toolkit-${VERSION}/launchers/start_session_gui.cmd"
test -f "releases/pluto-plus-sdr-toolkit-${VERSION}/launchers/start_live_spectrum_gui.cmd"
```

## 9. Release Manifest

```bash
test -f "releases/pluto-plus-sdr-toolkit-${VERSION}/MANIFEST.txt"
grep -n "pluto_scan_session.exe" "releases/pluto-plus-sdr-toolkit-${VERSION}/MANIFEST.txt"
grep -n "pluto_spectrum_stream.exe" "releases/pluto-plus-sdr-toolkit-${VERSION}/MANIFEST.txt"
grep -n "run_fm_scan.cmd" "releases/pluto-plus-sdr-toolkit-${VERSION}/MANIFEST.txt"
grep -n "run_noaa_scan.cmd" "releases/pluto-plus-sdr-toolkit-${VERSION}/MANIFEST.txt"
```

## 10. ZIP Existence

```bash
test -f "releases/pluto-plus-sdr-toolkit-${VERSION}.zip"
ls -lh "releases/pluto-plus-sdr-toolkit-${VERSION}.zip"
```
