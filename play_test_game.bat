@echo off
setlocal
cd /d "%~dp0"

echo.
echo  ============================================
echo   Encryptic Anti-Cheat - Test Game
echo  ============================================
echo.
echo  F1-F6 = built-in cheat triggers
echo  Right panel = live detection log
echo  Run with telemetry: keep the server window open
echo.

set "OUT=build\examples\test_game\Release"
set "NODE_SERVER=server\examples\node\server.js"

if not exist build (
    echo [1/3] Configuring CMake...
    cmake -B build -DENCRYPTIC_BUILD_SHARED=ON -DENCRYPTIC_BUILD_EXAMPLES=ON
    if errorlevel 1 goto :fail
)

echo [1/3] Building test game...
cmake --build build --config Release --target encryptic_test_game
if errorlevel 1 goto :fail

if not exist "%OUT%" (
    echo Build output not found at %OUT%
    goto :fail
)

echo [2/3] Copying config...
copy /Y "examples\test_game\encryptic_config.json" "%OUT%\encryptic_config.json" >nul

where node >nul 2>&1
if %errorlevel%==0 (
    powershell -NoProfile -Command "try { $c = New-Object System.Net.Sockets.TcpClient; $c.Connect('127.0.0.1', 8787); $c.Close(); exit 0 } catch { exit 1 }" >nul 2>&1
    if errorlevel 1 (
        echo [3/3] Starting telemetry server in new window...
        start "Encryptic Telemetry" cmd /k "cd /d %~dp0 && node %NODE_SERVER%"
        timeout /t 1 /nobreak >nul
    ) else (
        echo [3/3] Telemetry server already running on port 8787.
    )
) else (
    echo [3/3] Node.js not found — violations still show in-game, telemetry disabled.
    echo        Install Node.js or run: node server\examples\node\server.js
)

echo.
echo Launching Encryptic Test Game...
echo.
start "" /wait "%OUT%\encryptic_test_game.exe"

exit /b 0

:fail
echo.
echo Build failed. Install CMake + Visual Studio C++ tools, then run play_test_game.bat again.
pause
exit /b 1
