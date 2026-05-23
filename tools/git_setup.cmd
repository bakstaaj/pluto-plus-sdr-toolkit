@echo off
setlocal
where git >nul 2>nul
if errorlevel 1 (
    echo ERROR: git was not found in PATH.
    pause
    exit /b 1
)
if not exist ".git" (
    git init
)
git add .
git status
echo.
echo Next:
echo   git commit -m "Initial Pluto+ SDR Windows toolkit"
echo   git branch -M main
echo   git remote add origin https://github.com/bakstaaj/^<REPO-NAME^>.git
echo   git push -u origin main
pause
