@echo off
rem ABOUTME: Setup for the wotlane friends package: runs the setup.ps1 next to it (extracts the ELF from your ISO, prepares the runtime).
rem ABOUTME: Part of the package built by tools\package_friends.ps1.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup.ps1" %*
