@echo off
rem ABOUTME: Configura o build rapido do runtime (build_dev) com a camada nativa e o jogo traduzido do repo. Gerado por tools\setup_runtime.ps1.
rem ABOUTME: build_runtime.ps1 cria o configure_dev_nm.bat trocando o texto build_dev" do fim: o -B fica por ultimo.
cd /d "%~dp0"
call "@VCVARS@" @VCVARS_ARGS@ >nul
set "PATH=@CMAKE_BIN@;%PATH%"
cmake -S PS2Recomp -G Ninja -DCMAKE_BUILD_TYPE=Release -DWOTM_NATIVE_DIR=@REPO@/native -DPS2X_FAST_LINK=ON -DPS2X_ENABLE_AGRESSIVE_LOGS=OFF -DPS2X_ENABLE_IOP_RPC_TRACE=OFF -DPS2X_BUILD_RECOMP=OFF -DPS2X_BUILD_ANALYZER=OFF -DPS2X_BUILD_TEST=OFF -DPS2X_BUILD_STUDIO=OFF -DPS2X_ENABLE_DEBUG_UI=OFF -DPS2X_RUNNER_DIR=@REPO@/recomp/retail -B "%~dp0build_dev"
