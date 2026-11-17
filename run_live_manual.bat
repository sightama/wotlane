@echo off
rem Opens the recompiled game live with native rendering to play with the keyboard (-Auto 0: no automatic pad script).
rem Keyboard: arrows = analog stick, Space = X, Z = square, X = triangle, S = circle, Enter = Start, I/J/K/L = D-pad.
rem Other arguments go straight to run_live.bat (e.g. -Stats 1  or  -Build build).
call "%~dp0run_live.bat" -Auto 0 %*
exit /b %errorlevel%
