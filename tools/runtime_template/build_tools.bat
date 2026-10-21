@echo off
rem ABOUTME: Compila o analisador e o recompilador do PS2Recomp (ps2_analyzer, ps2_recomp) em build_tools.
rem ABOUTME: Gerado por tools\setup_runtime.ps1; so precisa rodar de novo se o PS2Recomp mudar.
cd /d "%~dp0"
call "@VCVARS@" >nul
set "PATH=@CMAKE_BIN@;%PATH%"
cmake -S PS2Recomp -G Ninja -DCMAKE_BUILD_TYPE=Release -DPS2X_BUILD_RUNTIME=OFF -DPS2X_BUILD_TEST=OFF -DPS2X_BUILD_STUDIO=OFF -B build_tools || exit /b 1
cmake --build build_tools
