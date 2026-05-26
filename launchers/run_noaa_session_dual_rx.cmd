@echo off
setlocal
cd /d "%~dp0\.."
set EXE=build\native\pluto_scan_session.exe
if not exist "%EXE%" (
  echo ERROR: %EXE% not found. Run tools\build_native_ucrt64.sh first from MSYS2.
  exit /b 1
)
"%EXE%" --band noaa --uri ip:192.168.2.1 --cycles 3 --rx-mode auto --rx-combine max --out-prefix noaa_dual_rx
endlocal
