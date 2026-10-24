@echo off
setlocal
cd /d "%~dp0"
if not exist "port\build\bt3.exe" (
    echo The native executable has not been built. See NATIVE_TAG_TEAM.md.
    pause
    exit /b 1
)
if not exist "gamedata-bt4\native-disc.json" (
    echo Import your BT4 B14 REV2 English disc with port\tools\extract_bt4.py first.
    echo See NATIVE_TAG_TEAM.md for the supported disc and instructions.
    pause
    exit /b 1
)
rem Do not inherit diagnostics or scripted input from a development session.
for /f "tokens=1 delims==" %%V in ('set BT3_ 2^>nul') do set "%%V="
set "BT3_DATA=gamedata-bt4"
set "BT3_TAG_TEAM=1"
set "BT3_GS=gpu"
set "BT3_MENU_ITEM4=0"
set "BT3_UPDATE_CHECK=0"
set "BT3_SAVES=port/build/native-bt4-prototype-saves"
set "BT3_SETTINGS=port/build/native-bt4-prototype-settings.txt"
echo Native BT4 Tag Team prototype - press Select at the main menu for Modded Modes.
echo F1: controller bindings and graphics. Right Shift is Select on the keyboard.
echo Flick right stick to change target. Tap C/L3 to target an attacker; hold to lock off.
echo BT4 uses separate saves and settings. This build is local and experimental.
start "" /wait "%~dp0port\build\bt3.exe"
if errorlevel 1 (
    echo The native game stopped with an error. Check bt3_crash.txt if it was created.
    pause
    exit /b 1
)
