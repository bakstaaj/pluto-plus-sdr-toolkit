# Pluto+ SDR Windows Toolkit

Native Windows/MSYS2 tools and WPF GUI wrappers for Pluto+ SDR scanning, activity monitoring, reporting, and live spectrum/waterfall display.

## Hardware assumption

This project defaults to the user's Pluto+ SDR configuration with receive coverage down to at least **70 MHz**. FM broadcast scanning is included as a first-class test target.

## Main workflows

```bash
./build/pluto_scan_session.exe --config configs/2m.conf
./build/pluto_scan_session.exe --config configs/fm.conf
./build/pluto_scan_session.exe --config configs/airband.conf
./build/pluto_scan_session.exe --config configs/noaa.conf
```

## Build native tools

From MSYS2 UCRT64:

```bash
cd ~/sdrdev/pluto_native_test
cmake -S . -B build -G Ninja
cmake --build build
```

## Git workflow

```bash
git status
git add .
git commit -m "Initial Pluto+ SDR Windows toolkit"
git branch -M main
git remote add origin https://github.com/bakstaaj/<REPO-NAME>.git
git push -u origin main
```

Replace `<REPO-NAME>` with the actual GitHub repository name you create under `bakstaaj`.
