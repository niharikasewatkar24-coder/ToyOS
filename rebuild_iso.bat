@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\build.ps1"
if errorlevel 1 exit /b %errorlevel%
echo.
echo Native Windows build complete: build\kernel.bin
echo Run run.bat to start QEMU and capture performance.jsonl.
