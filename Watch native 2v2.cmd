@echo off
setlocal
cd /d "%~dp0"
if not exist "port\build\bt3.exe" (
    echo The native executable has not been built. See NATIVE_TAG_TEAM.md.
    pause
    exit /b 1
)
if not exist "gamedata\" (
    echo Extract your own USA disc with the upstream installer first. See NATIVE_TAG_TEAM.md.
    pause
    exit /b 1
)
rem Clear inherited diagnostics and scripted input from development sessions.
for /f "tokens=1 delims==" %%V in ('set BT3_ 2^>nul') do set "%%V="
set "BT3_TAG_TEAM=1"
set "BT3_TAG_ROSTER=2,2"
set "BT3_DEMO=1"
set "BT3_GS=gpu"
set "BT3_DATA=gamedata"
set "BT3_MENU_ITEM4=0"
set "BT3_UPDATE_CHECK=0"
set "BT3_SAVES=port/build/native-prototype-saves"
set "BT3_SETTINGS=port/build/native-prototype-settings.txt"
echo Native four-fighter CPU demo. It closes when the fight finishes.
start "" /wait "%~dp0port\build\bt3.exe"
if errorlevel 1 (
    echo The native game stopped with an error. Check bt3_crash.txt if it was created.
    pause
    exit /b 1
)
