#!/usr/bin/env bash
set -euo pipefail

# repair_repo_layout.sh
#
# Run from repository root:
#   cd ~/sdrdev/pluto_native_test
#   bash repair_repo_layout.sh
#
# Fixes the first repo-layout migration issue where the old root CMakeLists.txt
# was left in place and still points at src/*.c.

echo "Repairing Pluto+ repository layout..."

mkdir -p native/src native configs launchers gui docs tools

# Move any remaining old src files into native/src.
if [ -d src ]; then
    for f in src/*.c src/*.h; do
        [ -e "$f" ] || continue
        dst="native/src/$(basename "$f")"
        if [ -e "$dst" ]; then
            echo "SKIP: $dst already exists"
        else
            echo "MOVE: $f -> $dst"
            mv "$f" "$dst"
        fi
    done
    rmdir src 2>/dev/null || true
fi

# Back up current root CMakeLists if it is not already the new root one.
if [ -f CMakeLists.txt ]; then
    if ! grep -q "add_subdirectory(native)" CMakeLists.txt; then
        backup="CMakeLists.pre-layout-repair.txt"
        if [ ! -f "$backup" ]; then
            echo "BACKUP: CMakeLists.txt -> $backup"
            cp CMakeLists.txt "$backup"
        fi
    fi
fi

# Force correct root CMakeLists.txt.
cat > CMakeLists.txt <<'EOF'
cmake_minimum_required(VERSION 3.20)

project(pluto_plus_sdr_toolkit C)

add_subdirectory(native)
EOF

# Force correct native/CMakeLists.txt.
cat > native/CMakeLists.txt <<'EOF'
cmake_minimum_required(VERSION 3.20)

project(pluto_native_tools C)

find_package(PkgConfig REQUIRED)

pkg_check_modules(LIBIIO REQUIRED IMPORTED_TARGET libiio)
pkg_check_modules(LIBAD9361 REQUIRED IMPORTED_TARGET libad9361)
pkg_check_modules(FFTW3 REQUIRED IMPORTED_TARGET fftw3)

add_executable(pluto_probe src/pluto_probe.c)
target_link_libraries(pluto_probe PRIVATE PkgConfig::LIBIIO)

add_executable(pluto_rx_capture src/pluto_rx_capture.c)
target_link_libraries(pluto_rx_capture PRIVATE PkgConfig::LIBIIO PkgConfig::LIBAD9361 m)

add_executable(pluto_iq_recorder src/pluto_iq_recorder.c)
target_link_libraries(pluto_iq_recorder PRIVATE PkgConfig::LIBIIO PkgConfig::LIBAD9361 m)

add_executable(pluto_fft_scanner src/pluto_fft_scanner.c)
target_link_libraries(pluto_fft_scanner PRIVATE PkgConfig::LIBIIO PkgConfig::LIBAD9361 PkgConfig::FFTW3 m)

add_executable(pluto_sweep_scanner src/pluto_sweep_scanner.c)
target_link_libraries(pluto_sweep_scanner PRIVATE PkgConfig::LIBIIO PkgConfig::LIBAD9361 PkgConfig::FFTW3 m)

add_executable(pluto_scan_group src/pluto_scan_group.c)

add_executable(pluto_band_scan src/pluto_band_scan.c)

add_executable(pluto_activity_monitor src/pluto_activity_monitor.c)
target_link_libraries(pluto_activity_monitor PRIVATE PkgConfig::LIBIIO PkgConfig::LIBAD9361 PkgConfig::FFTW3 m)

add_executable(pluto_activity_summary src/pluto_activity_summary.c)

add_executable(pluto_scan_session src/pluto_scan_session.c)

add_executable(pluto_session_report src/pluto_session_report.c)

add_executable(pluto_spectrum_stream src/pluto_spectrum_stream.c)
target_link_libraries(pluto_spectrum_stream PRIVATE PkgConfig::LIBIIO PkgConfig::LIBAD9361 PkgConfig::FFTW3 m)

if (MINGW)
    foreach(target
        pluto_probe
        pluto_rx_capture
        pluto_iq_recorder
        pluto_fft_scanner
        pluto_sweep_scanner
        pluto_scan_group
        pluto_band_scan
        pluto_activity_monitor
        pluto_activity_summary
        pluto_scan_session
        pluto_session_report
        pluto_spectrum_stream)
        target_compile_options(${target} PRIVATE -Wall -Wextra -O2)
    endforeach()
endif()
EOF

# Check expected source files.
missing=0
for f in \
    pluto_probe.c \
    pluto_rx_capture.c \
    pluto_iq_recorder.c \
    pluto_fft_scanner.c \
    pluto_sweep_scanner.c \
    pluto_scan_group.c \
    pluto_band_scan.c \
    pluto_activity_monitor.c \
    pluto_activity_summary.c \
    pluto_scan_session.c \
    pluto_session_report.c \
    pluto_spectrum_stream.c
do
    if [ ! -f "native/src/$f" ]; then
        echo "MISSING: native/src/$f"
        missing=1
    fi
done

if [ "$missing" -ne 0 ]; then
    echo
    echo "Some source files are missing from native/src."
    echo "Find them with:"
    echo "  find . -name 'pluto_*.c' -print"
    echo
    exit 1
fi

echo
echo "Source layout looks good."
echo
echo "Cleaning stale build directory..."
rm -rf build

echo
echo "Configuring and building..."
cmake -S . -B build -G Ninja
cmake --build build

echo
echo "Repair complete."
echo
echo "Native tools are now in:"
echo "  build/native/"
echo
echo "Test:"
echo "  ./build/native/pluto_scan_session.exe --config configs/2m.conf --dry-run"
