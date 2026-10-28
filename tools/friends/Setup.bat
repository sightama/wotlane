@echo off
rem ABOUTME: Setup do pacote wotlane para amigos: roda o setup.ps1 ao lado (tira o ELF da sua ISO e prepara o runtime).
rem ABOUTME: Faz parte do pacote do tools\package_friends.ps1.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup.ps1" %*
