@echo off
"%~dp0TaskbarSpeed.exe" /apply /quiet
if errorlevel 1 (
    type "%~dp0Status.txt"
    pause
    exit /b 1
)
"%~dp0TaskbarSpeed.exe" /startup
