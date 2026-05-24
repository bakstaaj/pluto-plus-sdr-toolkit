@echo off
setlocal

set "RELEASE_ROOT=%~dp0.."
set "BIN_DIR=%RELEASE_ROOT%\bin\native"

set "PATH=%BIN_DIR%;%PATH%"

if not exist "%BIN_DIR%\pluto_spectrum_stream.exe" (
    echo ERROR: pluto_spectrum_stream.exe not found:
    echo   %BIN_DIR%\pluto_spectrum_stream.exe
    pause
    exit /b 1
)

"%BIN_DIR%\pluto_spectrum_stream.exe" --freq 100000000 --rate 2000000 --fft 2048 --avg 2

pause
