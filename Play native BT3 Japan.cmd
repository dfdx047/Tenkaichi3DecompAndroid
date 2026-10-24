@echo off
setlocal
cd /d "%~dp0"
if not exist "port\build\bt3.exe" (
    echo The native executable has not been built. See NATIVE_TAG_TEAM.md.
    pause
    exit /b 1
)
if not exist "gamedata-jp\native-disc.json" (
    echo Import your disc with port\tools\import_native_disc.py first.
    echo Use gamedata-jp as the destination. See NATIVE_TAG_TEAM.md.
    pause
    exit /b 1
)
rem Clear inherited diagnostics and scripted input from development sessions.
for /f "tokens=1 delims==" %%V in ('set BT3_ 2^>nul') do set "%%V="
set "BT3_DATA=gamedata-jp"
set "BT3_TAG_TEAM=1"
set "BT3_GS=gpu"
set "BT3_MENU_ITEM4=0"
set "BT3_UPDATE_CHECK=0"
set "BT3_SAVES=port/build/native-jp-prototype-saves"
set "BT3_SETTINGS=port/build/native-jp-prototype-settings.txt"
echo Native BT3 Japan - press Select at the main menu for Modded Modes.
echo Choose your language at startup. F1 opens controller, graphics and language options.
echo Right Shift is Select on the keyboard. This is a local experimental build.
start "" /wait "%~dp0port\build\bt3.exe"
if errorlevel 1 (
    echo The native game stopped with an error. Check bt3_crash.txt if it was created.
    pause
    exit /b 1
)
