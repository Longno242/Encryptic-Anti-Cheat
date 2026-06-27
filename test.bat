@echo off
setlocal
cd /d "%~dp0"

echo.
echo  ============================================
echo   Encryptic Anti-Cheat - Watermark Preview
echo  ============================================
echo.

if not exist build (
    echo [1/2] Configuring CMake...
    cmake -B build -DENCRYPTIC_BUILD_SHARED=ON -DENCRYPTIC_BUILD_EXAMPLES=ON
    if errorlevel 1 goto :fail
) else (
    echo [1/2] Build folder found, rebuilding preview...
)

cmake --build build --config Release --target watermark_preview
if errorlevel 1 goto :fail

echo.
echo [2/2] Launching EAC-style launch watermark...
echo       Customize encryptic_watermark.json in the repo root.
echo.

copy /Y encryptic_watermark.json build\examples\watermark_preview\Release\encryptic_watermark.json >nul 2>&1
start "" /wait "build\examples\watermark_preview\Release\watermark_preview.exe"

echo Done.
exit /b 0

:fail
echo.
echo Build failed. Install CMake and Visual Studio C++ tools, then run test.bat again.
pause
exit /b 1
