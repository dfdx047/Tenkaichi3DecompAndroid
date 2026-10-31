@echo off
setlocal
cd /d "%~dp0"
py -3 port\tools\native_play.py
if errorlevel 1 pause
