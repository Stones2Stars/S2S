@echo off
REM ===========================================================================
REM  agentstart.bat - start the Stones2Stars test game unattended.
REM  Closes any running game, then launches BTS with the S2S mod + the test save.
REM  NO build/MakeDll step - it only closes and starts the game.
REM
REM  Machine-specific paths come from .env (gitignored) next to this file:
REM      S2S_BTS_DIR   = folder containing Civ4BeyondSword.exe
REM      S2S_MOD_NAME  = mod folder under <BTS>\Mods\   (e.g. Stones2Stars)
REM      S2S_SAVE_GAME = save filename in My Games\...\Saves\single\
REM ===========================================================================
setlocal

set "ENVFILE=%~dp0.env"
if not exist "%ENVFILE%" (
    echo [agentstart] ERROR: .env not found at "%ENVFILE%"  ^(copy .env.example to .env^)
    exit /b 1
)

for /f "usebackq eol=# tokens=1,* delims==" %%A in ("%ENVFILE%") do (
    if /i "%%A"=="S2S_BTS_DIR"   set "BTS_DIR=%%B"
    if /i "%%A"=="S2S_MOD_NAME"  set "MOD=%%B"
    if /i "%%A"=="S2S_SAVE_GAME" set "SAVE=%%B"
)

if not defined BTS_DIR ( echo [agentstart] ERROR: S2S_BTS_DIR missing in .env & exit /b 1 )
if not defined MOD     ( echo [agentstart] ERROR: S2S_MOD_NAME missing in .env & exit /b 1 )
if not defined SAVE    ( echo [agentstart] ERROR: S2S_SAVE_GAME missing in .env & exit /b 1 )

set "EXE=%BTS_DIR%\Civ4BeyondSword.exe"
set "SAVEPATH=%USERPROFILE%\Documents\My Games\Beyond the Sword\Saves\single\%SAVE%"

if not exist "%EXE%"      ( echo [agentstart] ERROR: exe not found:  "%EXE%"      & exit /b 1 )
if not exist "%SAVEPATH%" ( echo [agentstart] ERROR: save not found: "%SAVEPATH%" & exit /b 1 )

echo [agentstart] closing any running game...
taskkill /IM Civ4BeyondSword.exe /F >nul 2>&1
ping 127.0.0.1 -n 4 >nul

REM DEFAULT = SKIP the dev DLL's boot-time rebuild (_BootDLLCheck); load the deployed DLL
REM as-is. Safest for an unattended agent: if a boot build FAILS and we hadn't skipped, the
REM game is stuck on a blocking "press any key" window we cannot dismiss. Pass "bootcheck"
REM (agentstart.bat bootcheck) to instead let the boot-check run, from the correct place
REM (the cd below). Requires the getenv gate in CvGameCoreDLL.cpp DllMain.
if /i not "%~1"=="bootcheck" set "S2S_SKIP_BOOTCHECK=1"

REM Launch from the repo root (%~dp0) so the dev DLL's boot-check runs from the correct place.
cd /d "%~dp0"
echo [agentstart] launching: "mod= mods\%MOD%"  +  %SAVE%
start "" "%EXE%" "mod= mods\%MOD%" /FXSLOAD="%SAVEPATH%"

set "S2S_SKIP_BOOTCHECK="

endlocal
