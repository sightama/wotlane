@echo off
rem Compila o launcher (WotM_Launcher.exe na raiz do repo) com o csc do .NET Framework que vem no Windows. Icone: native\wotm.ico.
set "CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
if not exist "%CSC%" set "CSC=%WINDIR%\Microsoft.NET\Framework\v4.0.30319\csc.exe"
"%CSC%" /nologo /target:winexe /optimize+ /out:"%~dp0..\..\WotM_Launcher.exe" /r:System.Windows.Forms.dll /r:System.Drawing.dll /win32icon:"%~dp0..\..\native\wotm.ico" "%~dp0WotmLauncher.cs" "%~dp0LobbyClient.cs"
