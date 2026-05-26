@echo off
setlocal

set SCRIPT_DIR=%~dp0
set REPO_ROOT=%SCRIPT_DIR%..
set EXE=%REPO_ROOT%\build\native\pluto_activity_monitor.exe

if not exist "%EXE%" (
  echo ERROR: %EXE% not found.
  echo Build first from MSYS2 UCRT64: ./tools/build_native_ucrt64.sh
  exit /b 1
)

"%EXE%" --uri ip:192.168.2.1 --freq-file "%REPO_ROOT%\configs\activity_rx_test_freqs.csv" --rx-mode auto --rx-combine max --threshold-dbfs -55 --csv "%REPO_ROOT%\activity_monitor_production_rx_test.csv" --verbose
endlocal
