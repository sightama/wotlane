@echo off
rem Builds the native viewer with MSVC (BuildTools 2022). Needs raylib already built:
rem   set RAYLIB_DIR=<folder with include\raylib.h and raylib.lib>   (default: the build in the runtime's test clone)
setlocal
if "%RAYLIB_DIR%"=="" set "RAYLIB_DIR=C:\Users\TwistZero\wotm-recomp-win\build\_deps\raylib-build\raylib"
set "ProgramFiles(x86)=C:\Program Files (x86)"
set "PATH=C:\Program Files (x86)\Microsoft Visual Studio\Installer;%PATH%"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist "%~dp0..\build" mkdir "%~dp0..\build"
cl /nologo /std:c++17 /O2 /EHsc /MD /I"%RAYLIB_DIR%\include" "%~dp0viewer.cpp" /Fo"%~dp0..\build\viewer.obj" /Fe"%~dp0..\build\viewer.exe" /link /LIBPATH:"%RAYLIB_DIR%" raylib.lib winmm.lib gdi32.lib user32.lib shell32.lib opengl32.lib
rem monview (3D monster straight from the disc, native/wotm_model.hpp)
cl /nologo /std:c++17 /O2 /EHsc /MD /I"%RAYLIB_DIR%\include" "%~dp0monview.cpp" /Fo"%~dp0..\build\monview.obj" /Fe"%~dp0..\build\monview.exe" /link /LIBPATH:"%RAYLIB_DIR%" raylib.lib winmm.lib gdi32.lib user32.lib shell32.lib opengl32.lib
rem sndplay (native sound, native/wotm_snd.hpp): standard library only
cl /nologo /std:c++17 /O2 /EHsc /MD "%~dp0sndplay.cpp" /Fo"%~dp0..\build\sndplay.obj" /Fe"%~dp0..\build\sndplay.exe"
rem fmvplay (.PSS movies, native/wotm_fmv.hpp) only with FFmpeg: the same package the runtime's CMake downloads (ffmpeg-n7.1-*-lgpl-amd64-shared).
rem   set FFMPEG_DIR=<folder with include\ and bin\avcodec.lib>; the DLLs from bin\ go to build\ next to the exe
if not "%FFMPEG_DIR%"=="" (
  cl /nologo /std:c++17 /O2 /EHsc /MD /I"%RAYLIB_DIR%\include" /I"%FFMPEG_DIR%\include" "%~dp0fmvplay.cpp" /Fo"%~dp0..\build\fmvplay.obj" /Fe"%~dp0..\build\fmvplay.exe" /link /LIBPATH:"%RAYLIB_DIR%" /LIBPATH:"%FFMPEG_DIR%\bin" raylib.lib avcodec.lib avutil.lib winmm.lib gdi32.lib user32.lib shell32.lib opengl32.lib
  copy /y "%FFMPEG_DIR%\bin\*.dll" "%~dp0..\build\" >nul
)
endlocal
