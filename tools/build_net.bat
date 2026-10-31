@echo off
rem ABOUTME: Builds the netcode without the game (native\net: wotm_server and wotm_net_test) in native\net\build and runs the tests.
rem ABOUTME: Usage: tools\build_net.bat [notest]. Requires VS 2022 Build Tools (MSVC) and CMake 3.24+.
setlocal
set "REPO=%~dp0.."
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
    echo Could not find VS 2022 Build Tools at "%VCVARS%".
    exit /b 1
)
set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%"
call "%VCVARS%" >nul
if exist "C:\Program Files\CMake\bin\cmake.exe" set "PATH=C:\Program Files\CMake\bin;%PATH%"
set "BUILD=%REPO%\native\net\build"
if not exist "%BUILD%\build.ninja" (
    cmake -S "%REPO%\native\net" -B "%BUILD%" -G Ninja -DCMAKE_BUILD_TYPE=Release || exit /b 1
)
cmake --build "%BUILD%" || exit /b 1
if /i "%~1"=="notest" exit /b 0
"%BUILD%\wotm_net_test.exe"
