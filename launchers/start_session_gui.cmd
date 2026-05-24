@echo off
setlocal

REM Repo-level launcher for the Pluto+ Session GUI.
REM This file is intended to live in:
REM   launchers\start_session_gui.cmd
REM
REM It assumes the GUI source folder is:
REM   gui\PlutoGuiStarter_v3_RepoLayout
REM
REM It runs the GUI using dotnet run.

set "PROJECT_DIR=%~dp0.."
set "GUI_DIR=%PROJECT_DIR%\gui\PlutoGuiStarter_v3_RepoLayout"

if not exist "%GUI_DIR%\PlutoGuiStarter.csproj" (
    echo ERROR: Session GUI project was not found:
    echo   %GUI_DIR%\PlutoGuiStarter.csproj
    echo.
    echo Make sure gui\PlutoGuiStarter_v3_RepoLayout exists.
    pause
    exit /b 1
)

cd /d "%GUI_DIR%"

echo Starting Pluto+ Session GUI...
echo Project:
echo   %GUI_DIR%
echo.

dotnet run

if errorlevel 1 (
    echo.
    echo ERROR: Session GUI failed to start.
    pause
    exit /b 1
)

exit /b 0
