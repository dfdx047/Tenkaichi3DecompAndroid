@echo off
setlocal
cd /d "%~dp0"
if exist "port\build\native-setup\Setup.exe" (
    start "" "port\build\native-setup\Setup.exe"
) else (
    py -3 port\tools\native_setup.py
    if errorlevel 1 pause
)
