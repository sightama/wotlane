@echo off
rem ABOUTME: Compila o netcode sem o jogo (native\net: wotm_server e wotm_net_test) em native\net\build e roda os testes.
rem ABOUTME: Uso: tools\build_net.bat [notest]. Precisa do VS 2022 Build Tools (MSVC) e do CMake 3.24+.
setlocal
set "REPO=%~dp0.."
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
    echo Nao achei o VS 2022 Build Tools em "%VCVARS%".
    exit /b 1
)
call "%VCVARS%" >nul
if exist "C:\Program Files\CMake\bin\cmake.exe" set "PATH=C:\Program Files\CMake\bin;%PATH%"
set "BUILD=%REPO%\native\net\build"
if not exist "%BUILD%\build.ninja" (
    cmake -S "%REPO%\native\net" -B "%BUILD%" -G Ninja -DCMAKE_BUILD_TYPE=Release || exit /b 1
)
cmake --build "%BUILD%" || exit /b 1
if /i "%~1"=="notest" exit /b 0
"%BUILD%\wotm_net_test.exe"
