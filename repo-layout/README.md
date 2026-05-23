# Pluto+ SDR Windows Toolkit

Native Windows/MSYS2 tools and WPF GUI wrappers for Pluto+ SDR scanning, activity monitoring, session reports, live spectrum, waterfall, and peak display.

## Hardware target

This project targets the user's Pluto+ SDR setup and assumes RX coverage down to at least 70 MHz. FM broadcast scanning, 88-108 MHz, is a first-class test preset.

## Build native tools

From MSYS2 UCRT64:

```bash
./tools/build_native_ucrt64.sh
```

Executables are built under:

```text
build/native/
```

## Scan examples

```bash
./build/native/pluto_scan_session.exe --config configs/2m.conf
./build/native/pluto_scan_session.exe --config configs/fm.conf
./build/native/pluto_scan_session.exe --config configs/airband.conf
./build/native/pluto_scan_session.exe --config configs/noaa.conf
```

## Live spectrum examples

```bash
./build/native/pluto_spectrum_stream.exe --freq 146520000 --rate 1000000 --fft 1024
./build/native/pluto_spectrum_stream.exe --freq 100000000 --rate 2000000 --fft 2048
```
