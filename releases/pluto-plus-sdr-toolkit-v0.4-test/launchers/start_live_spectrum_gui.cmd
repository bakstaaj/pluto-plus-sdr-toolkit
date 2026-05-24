@echo off
setlocal

set "RELEASE_ROOT=%~dp0.."
set "GUI_EXE=%RELEASE_ROOT%\gui\PlutoLiveSpectrumGui\PlutoLiveSpectrumGui.exe"

if exist "%GUI_EXE%" (
    start "" "%GUI_EXE%"
    exit /b 0
)

echo Live Spectrum GUI executable was not found.
echo Expected:
echo   %GUI_EXE%
echo.
echo If the GUI was not published, run the source GUI with dotnet run.
pause
