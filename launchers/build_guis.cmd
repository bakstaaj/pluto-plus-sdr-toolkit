@echo off
setlocal

REM Builds both WPF GUI projects from the repo-level launchers folder.

set "PROJECT_DIR=%~dp0.."

set "SESSION_GUI=%PROJECT_DIR%\gui\PlutoGuiStarter_v3_RepoLayout"
set "SPECTRUM_GUI=%PROJECT_DIR%\gui\PlutoLiveSpectrumGui_v5_RepoLayout_FM"

if exist "%SESSION_GUI%\PlutoGuiStarter.csproj" (
    echo Building Session GUI...
    cd /d "%SESSION_GUI%"
    dotnet build
    if errorlevel 1 (
        echo ERROR: Session GUI build failed.
        pause
        exit /b 1
    )
) else (
    echo WARNING: Session GUI project not found:
    echo   %SESSION_GUI%
)

echo.

if exist "%SPECTRUM_GUI%\PlutoLiveSpectrumGui.csproj" (
    echo Building Live Spectrum GUI...
    cd /d "%SPECTRUM_GUI%"
    dotnet build
    if errorlevel 1 (
        echo ERROR: Live Spectrum GUI build failed.
        pause
        exit /b 1
    )
) else (
    echo WARNING: Live Spectrum GUI project not found:
    echo   %SPECTRUM_GUI%
)

echo.
echo GUI builds complete.
pause
