@echo off
setlocal

REM Opens common Pluto+ report files if they exist.
set "PROJECT_DIR=%~dp0"
cd /d "%PROJECT_DIR%"

for %%F in (2m_report.html airband_report.html noaa_report.html 70cm_report.html fm_report.html ism915_report.html) do (
    if exist "%%F" (
        echo Opening %%F
        start "" "%%F"
    )
)

echo.
echo Done.
pause
