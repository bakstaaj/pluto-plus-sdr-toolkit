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

add_executable(pluto_audio_monitor src/pluto_audio_monitor.c)
target_link_libraries(pluto_audio_monitor PRIVATE PkgConfig::LIBIIO PkgConfig::LIBAD9361 m)

if (MINGW)
    target_compile_options(pluto_audio_monitor PRIVATE -Wall -Wextra -O2)
endif()
