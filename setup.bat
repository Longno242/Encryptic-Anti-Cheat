@echo off
setlocal
cd /d "%~dp0"

echo.
echo  ============================================================
echo   ENCRYPTIC ANTI-CHEAT - SETUP WIZARD
echo   Connects client guards, telemetry, heartbeat, and your game
echo  ============================================================
echo.

where powershell >nul 2>&1
if errorlevel 1 (
    echo PowerShell is required. Install it and run setup.bat again.
    pause
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup\Encryptic-Setup.ps1"
set ERR=%ERRORLEVEL%

if %ERR% NEQ 0 (
    echo.
    echo Setup failed with code %ERR%.
    pause
    exit /b %ERR%
)

echo.
echo Setup complete. See dist\ folder for your game package.
pause
exit /b 0
