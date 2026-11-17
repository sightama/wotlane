@echo off
rem Opens War of the Monsters in PCSX2 with the chosen ELF (default: halfcpp). Examples: jogar.bat   |   jogar.bat bis_old   |   jogar.bat control_matching
set NAME=%1
if "%NAME%"=="" set NAME=halfcpp
start "" "D:\Emulators\PCSX2\pcsx2-qt.exe" -batch -elf "%~dp0build\pcsx2_test\SCUS_971.97_%NAME%.elf" -- "%~dp0ISO\SCUS_971.97.War of the Monsters.iso"
