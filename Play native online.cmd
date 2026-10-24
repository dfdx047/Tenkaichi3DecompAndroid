@echo off
setlocal
cd /d "%~dp0"
if not exist "port\build\bt3.exe" (
    echo Build the native prototype first. See NATIVE_TAG_TEAM.md.
    pause
    exit /b 1
)
for /f "tokens=1 delims==" %%V in ('set BT3_ 2^>nul') do set "%%V="
set "BT3_TAG_TEAM=1"
set "BT3_TAG_ONLINE=1"
set "BT3_GS=gpu"
set "BT3_UPDATE_CHECK=0"
set "BT3_DATA=gamedata"
set "BT3_SAVES=port/build/native-online-saves"
set "BT3_SETTINGS=port/build/native-online-settings.txt"
echo Local native online prototype: up to ten players and ten spectators, with CPU slots.
echo Use the same native build and game data on every PC. Host or join in the game window.
start "" /wait "%~dp0port\build\bt3.exe"
if errorlevel 1 pause
