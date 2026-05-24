@echo off
setlocal

REM Repo-level launcher for the Pluto+ Live Spectrum GUI.
REM This file is intended to live in:
REM   launchers\start_live_spectrum_gui.cmd
REM
REM It assumes the GUI source folder is:
REM   gui\PlutoLiveSpectrumGui_v5_RepoLayout_FM
REM
REM It runs the GUI using dotnet run.

set "PROJECT_DIR=%~dp0.."
set "GUI_DIR=%PROJECT_DIR%\gui\PlutoLiveSpectrumGui_v5_RepoLayout_FM"

if not exist "%GUI_DIR%\PlutoLiveSpectrumGui.csproj" (
    echo ERROR: Live Spectrum GUI project was not found:
    echo   %GUI_DIR%\PlutoLiveSpectrumGui.csproj
    echo.
    echo Make sure gui\PlutoLiveSpectrumGui_v5_RepoLayout_FM exists.
    pause
    exit /b 1
)

cd /d "%GUI_DIR%"

echo Starting Pluto+ Live Spectrum GUI...
echo Project:
echo   %GUI_DIR%
echo.

dotnet run

if errorlevel 1 (
    echo.
    echo ERROR: Live Spectrum GUI failed to start.
    pause
    exit /b 1
)

exit /b 0
