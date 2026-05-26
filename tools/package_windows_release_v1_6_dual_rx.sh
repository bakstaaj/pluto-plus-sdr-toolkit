#!/usr/bin/env bash
set -euo pipefail

# Pluto+ SDR Toolkit Windows Release Packager v1.6
# Run from the repository root in MSYS2 UCRT64:
#   cd ~/sdrdev/pluto_native_test
#   /c/Users/jim/Downloads/PlutoReleaseV16Packager/tools/package_windows_release_v1_6_dual_rx.sh

RELEASE_NAME="pluto-plus-sdr-toolkit-v1.6-dual-rx-windows"
RELEASE_ROOT="releases/${RELEASE_NAME}"
ZIP_PATH="releases/${RELEASE_NAME}.zip"

if [[ ! -d native/src || ! -f native/CMakeLists.txt ]]; then
  echo "ERROR: Run this script from the repo root, for example:" >&2
  echo "  cd ~/sdrdev/pluto_native_test" >&2
  exit 1
fi

if [[ ! -x tools/build_native_ucrt64.sh ]]; then
  echo "ERROR: tools/build_native_ucrt64.sh not found or not executable." >&2
  exit 1
fi

echo "=== Building Windows native tools ==="
./tools/build_native_ucrt64.sh

mkdir -p releases
rm -rf "${RELEASE_ROOT}" "${ZIP_PATH}"
mkdir -p \
  "${RELEASE_ROOT}/bin" \
  "${RELEASE_ROOT}/configs" \
  "${RELEASE_ROOT}/docs" \
  "${RELEASE_ROOT}/launchers" \
  "${RELEASE_ROOT}/tools" \
  "${RELEASE_ROOT}/sessions"

copy_if_exists() {
  local src="$1"
  local dst="$2"
  if [[ -e "$src" ]]; then
    cp -a "$src" "$dst"
  else
    echo "WARN: missing ${src}"
  fi
}

copy_exe() {
  local name="$1"
  local src="build/native/${name}.exe"
  if [[ -f "$src" ]]; then
    echo "  exe: ${name}.exe"
    cp -a "$src" "${RELEASE_ROOT}/bin/"
  else
    echo "WARN: missing executable ${src}"
  fi
}

echo "=== Copying executables ==="
for exe in \
  pluto_probe \
  pluto_rx_capture \
  pluto_iq_recorder \
  pluto_fft_scanner \
  pluto_sweep_scanner \
  pluto_scan_group \
  pluto_band_scan \
  pluto_activity_monitor \
  pluto_activity_summary \
  pluto_scan_session \
  pluto_session_report \
  pluto_spectrum_stream \
  pluto_audio_monitor \
  pluto_audio_report \
  pluto_dual_rx_probe \
  pluto_dual_rx_power_scan; do
  copy_exe "$exe"
done

echo "=== Copying MSYS2/UCRT64 DLL dependencies ==="
# Copy dependent DLLs that live under /ucrt64/bin. This keeps the release portable
# on Windows machines that may not have the same MSYS2 PATH configured.
tmpdlls="$(mktemp)"
for exe in "${RELEASE_ROOT}/bin"/*.exe; do
  [[ -f "$exe" ]] || continue
  ldd "$exe" 2>/dev/null | awk '
    /\/ucrt64\/bin\// {
      for (i=1; i<=NF; i++) {
        if ($i ~ /^\/ucrt64\/bin\/.*\.dll$/) print $i;
      }
    }
  ' >> "$tmpdlls" || true
done
sort -u "$tmpdlls" | while read -r dll; do
  [[ -n "$dll" && -f "$dll" ]] || continue
  base="$(basename "$dll")"
  if [[ ! -f "${RELEASE_ROOT}/bin/${base}" ]]; then
    echo "  dll: ${base}"
    cp -a "$dll" "${RELEASE_ROOT}/bin/"
  fi
done
rm -f "$tmpdlls"

# A few runtime DLLs sometimes do not appear in ldd output depending on the build.
for dll in \
  /ucrt64/bin/libwinpthread-1.dll \
  /ucrt64/bin/libgcc_s_seh-1.dll \
  /ucrt64/bin/libstdc++-6.dll; do
  [[ -f "$dll" ]] && cp -an "$dll" "${RELEASE_ROOT}/bin/" || true
done

echo "=== Copying configs ==="
if [[ -d configs ]]; then
  find configs -maxdepth 1 -type f \( -name '*.csv' -o -name '*.conf' -o -name '*.json' \) -print0 \
    | xargs -0 -r cp -a -t "${RELEASE_ROOT}/configs/"
fi

echo "=== Copying launchers ==="
if [[ -d launchers ]]; then
  find launchers -maxdepth 1 -type f \( -name '*.cmd' -o -name '*.bat' -o -name '*.ps1' \) -print0 \
    | xargs -0 -r cp -a -t "${RELEASE_ROOT}/launchers/"
fi

# Make sure the new dual-RX launcher is included when present.
copy_if_exists "launchers/run_noaa_session_dual_rx.cmd" "${RELEASE_ROOT}/launchers/"
copy_if_exists "launchers/run_activity_monitor_dual_rx.cmd" "${RELEASE_ROOT}/launchers/"

echo "=== Copying docs ==="
if [[ -d docs ]]; then
  find docs -maxdepth 1 -type f \( -name '*.md' -o -name '*.txt' -o -name '*.html' \) -print0 \
    | xargs -0 -r cp -a -t "${RELEASE_ROOT}/docs/"
fi

# Include selected helper scripts for reference/use from MSYS2.
for tool in \
  tools/run_activity_monitor_production_rx_test_msys2.sh \
  tools/run_remaining_rx_dryrun_msys2.sh \
  tools/run_sweep_dual_rx_test_msys2.sh \
  tools/package_windows_release_v1_6_dual_rx.sh; do
  copy_if_exists "$tool" "${RELEASE_ROOT}/tools/"
done

echo "=== Creating release README ==="
cat > "${RELEASE_ROOT}/README.txt" <<README
Pluto+ SDR Toolkit v1.6 — Dual RX Windows Release
==================================================

This release is for Windows/MSYS2-side applications that connect to the Pluto+
over IIO using --uri ip:192.168.2.1.

This release does not include Pluto-local ARM binaries.

Key dual-RX tools/options:

  pluto_dual_rx_power_scan.exe
  pluto_activity_monitor.exe
  pluto_scan_session.exe
  pluto_band_scan.exe
  pluto_sweep_scanner.exe

Dual RX options:

  --rx-mode auto|single|dual
  --rx-combine max|average|separate

Recommended defaults:

  --rx-mode auto
  --rx-combine max

Meaning:

  auto     Use dual RX when voltage0/1/2/3 are available, otherwise RX1 only.
  single   Force RX1 only.
  dual     Require RX1 and RX2.
  max      Use the stronger of RX1/RX2 as the combined level.
  average  Average RX1/RX2 as the combined level.
  separate Log/report RX1/RX2 separately when the tool supports it.

Example tests from MSYS2 UCRT64, launched from the repo root:

  ./build/native/pluto_dual_rx_power_scan.exe \\
    --uri ip:192.168.2.1 \\
    --freq-file configs/dual_rx_test_freqs.csv \\
    --rx-mode auto \\
    --rx-combine max \\
    --threshold-dbfs -55 \\
    --csv dual_rx_power_scan_host.csv \\
    --verbose

  ./build/native/pluto_sweep_scanner.exe \\
    --uri ip:192.168.2.1 \\
    --start 162400000 \\
    --stop 162550000 \\
    --step 25000 \\
    --rx-mode auto \\
    --rx-combine max

Generated by: tools/package_windows_release_v1_6_dual_rx.sh
README

# Add a simple Windows launcher note if no top-level launcher exists.
cat > "${RELEASE_ROOT}/launchers/README_LAUNCHERS.txt" <<README
Launchers
=========

Run .cmd launchers from Windows Explorer or Command Prompt.
For command-line testing and development, MSYS2 UCRT64 from the repo root is still recommended.

Important dual-RX launcher files when present:

  run_noaa_session_dual_rx.cmd
  run_activity_monitor_dual_rx.cmd
README

# Inventory is useful for debugging what got packaged.
echo "=== Writing inventory ==="
(
  cd "${RELEASE_ROOT}"
  find . -type f | sort
) > "${RELEASE_ROOT}/MANIFEST.txt"

# Quick sanity checks for the v1.6 dual-RX executables.
echo "=== Sanity checks ==="
missing=0
for exe in \
  pluto_dual_rx_power_scan.exe \
  pluto_activity_monitor.exe \
  pluto_scan_session.exe \
  pluto_band_scan.exe \
  pluto_sweep_scanner.exe; do
  if [[ ! -f "${RELEASE_ROOT}/bin/${exe}" ]]; then
    echo "ERROR: expected ${exe} is missing from release/bin" >&2
    missing=1
  fi
done
if [[ "$missing" -ne 0 ]]; then
  echo "Release folder was created but required v1.6 executables are missing." >&2
  exit 1
fi

if command -v zip >/dev/null 2>&1; then
  echo "=== Creating ZIP with MSYS2 zip ==="
  (cd releases && zip -r "${RELEASE_NAME}.zip" "${RELEASE_NAME}")
else
  echo "=== zip not found; using PowerShell Compress-Archive ==="
  powershell.exe -NoProfile -Command \
    "Compress-Archive -Path '$(cygpath -w "${RELEASE_ROOT}")' -DestinationPath '$(cygpath -w "${ZIP_PATH}")' -Force"
fi

echo
echo "Created release folder: ${RELEASE_ROOT}"
echo "Created release zip:    ${ZIP_PATH}"
echo
echo "Recommended next commands:"
echo "  ls -lh ${ZIP_PATH}"
echo "  unzip -l ${ZIP_PATH} | head -80"
echo "  git status --short"
