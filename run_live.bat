@echo off
rem Runs the recompiled game live with native rendering, finding the runtime folder (the one with run_live.ps1) by itself.
rem Usage: run_live.bat [-Auto 0|1] [-Stats 1] [-Build build_dev|build]
rem   -Auto 0 = you play with the keyboard (script default: 1 = automatic pad script up to the stage)
rem Search order: the WOTM_RECOMP_DIR variable, .wotm_recomp_dir (saved the first time), this folder, sibling folders of this one
rem (wotm-recomp-win and any other that has run_live.ps1). Writes nothing from the game into the repo.
setlocal enableextensions
set "REPO=%~dp0"
if "%REPO:~-1%"=="\" set "REPO=%REPO:~0,-1%"
set "FOUND="

if defined WOTM_RECOMP_DIR if exist "%WOTM_RECOMP_DIR%\run_live.ps1" set "FOUND=%WOTM_RECOMP_DIR%"

rem set /p and the use of SAVED on separate lines: inside a ( ) block %SAVED% is expanded before set /p runs
if not defined FOUND if exist "%REPO%\.wotm_recomp_dir" set /p SAVED=<"%REPO%\.wotm_recomp_dir"
if not defined FOUND if defined SAVED call :check "%SAVED%"
if not defined FOUND call :check "%REPO%"
if not defined FOUND call :check "%REPO%\..\wotm-recomp-win"
if not defined FOUND for /d %%D in ("%REPO%\..\*") do if not defined FOUND call :check "%%~fD"

if not defined FOUND (
    echo Could not find run_live.ps1 near "%REPO%".
    set /p ASKED=Paste the path of the runtime folder ^(the one with run_live.ps1^):
    call :check "%ASKED%"
    if not defined FOUND (
        echo Invalid folder.
        exit /b 1
    )
)
> "%REPO%\.wotm_recomp_dir" echo %FOUND%

echo Runtime: %FOUND%
set "WOTM_REPO=%REPO%"
rem Debug snapshots (F7 pause, F6 frame advance, F8 snapshot with a note) on by default, as in the launcher
if not defined WOTM_FOTOS set "WOTM_FOTOS=1"
powershell -NoProfile -ExecutionPolicy Bypass -File "%FOUND%\run_live.ps1" %*
exit /b %errorlevel%

:check
rem %1 = candidate folder; sets FOUND if it has run_live.ps1
if exist "%~1\run_live.ps1" set "FOUND=%~f1"
exit /b 0
