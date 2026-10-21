@echo off
rem ABOUTME: Compila e roda o teste do cliente de lobby do launcher (LobbyTest.cs + LobbyClient.cs) contra o wotm_server.
rem ABOUTME: Precisa do native\net\build\wotm_server.exe (tools\build_net.bat). Sai com 0 se passar.
setlocal
set "CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
if not exist "%CSC%" set "CSC=%WINDIR%\Microsoft.NET\Framework\v4.0.30319\csc.exe"
set "OUT=%~dp0..\..\native\net\build\LobbyTest.exe"
"%CSC%" /nologo /target:exe /optimize+ /out:"%OUT%" "%~dp0LobbyTest.cs" "%~dp0LobbyClient.cs" || exit /b 1
"%OUT%" "%~dp0..\..\native\net\build\wotm_server.exe"
