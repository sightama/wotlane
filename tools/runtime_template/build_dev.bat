@echo off
rem ABOUTME: Compila o jogo (wotm.exe) e o executavel de teste (ps2EntryRunner.exe) em build_dev; chamado por build_runtime.bat.
rem ABOUTME: Gerado por tools\setup_runtime.ps1; o log vai para build_dev.log, que o build_runtime.ps1 acompanha.
cd /d "%~dp0"
call "@VCVARS@" @VCVARS_ARGS@ >nul
set "PATH=@CMAKE_BIN@;%PATH%"
cmake --build build_dev --target ps2EntryRunner wotm > build_dev.log 2>&1
