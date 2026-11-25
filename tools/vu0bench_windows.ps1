# ABOUTME: Checks the native VU0 microprograms built with MSVC on Windows x86 against VU0 calls recorded on another machine: builds
# ABOUTME: tools\vu0bench.cpp with the runtime's float-deciding flags and compares every result with that machine's (vu0bench --expect).
#
# Usage (an "x64 Native Tools Command Prompt for VS" or a Developer PowerShell, so cl.exe is on PATH):
#   powershell -File tools\vu0bench_windows.ps1 -Kept <folder>
#   -Kept: the folder `sh tools/vu0bench.sh --keep <folder>` filled on the Mac (vu0_code.bin, vu0_samples.bin, native.results). The
#     recordings hold game state: copy them by hand between your own machines, never into the repo, an issue or CI.
# The microprograms are generated here from your ELF (disc\SCUS_971.97, else <runtime>\discroot) with this checkout's
# tools\vu0native.py, so check out the same commit the Mac recorded with. Exits 0 when every result matches.
param([Parameter(Mandatory = $true)][string]$Kept)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
if (-not (Test-Path -LiteralPath $Kept)) { throw "No folder $Kept" }
$Kept = Convert-Path -LiteralPath $Kept   # vu0bench.exe gets the paths too: absolute, whatever PowerShell's location is
foreach ($f in 'vu0_code.bin', 'vu0_samples.bin', 'native.results') {
    if (-not (Test-Path (Join-Path $Kept $f))) { throw "$Kept has no $f (sh tools/vu0bench.sh --keep <folder> on the Mac fills it)" }
}
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) { throw 'cl.exe is not on PATH: run this from an "x64 Native Tools Command Prompt for VS" or a Developer PowerShell' }
# the runtime is an x64 build; a prompt set up for x86 would build something else (VsDevCmd names its target in VSCMD_ARG_TGT_ARCH)
if ($env:VSCMD_ARG_TGT_ARCH -and $env:VSCMD_ARG_TGT_ARCH -ne 'x64') { throw "cl.exe targets $env:VSCMD_ARG_TGT_ARCH: use an x64 Native Tools prompt (the runtime is x64)" }
$runtime = if ($env:WOTM_RECOMP_DIR) { $env:WOTM_RECOMP_DIR } elseif (Test-Path "$repo\.wotm_recomp_dir") { (Get-Content "$repo\.wotm_recomp_dir" -TotalCount 1).Trim() } else { '' }
if (-not $runtime -or -not (Test-Path "$runtime\PS2Recomp\ps2xRuntime\include")) { throw 'Runtime folder not found: run run_live.bat once or set WOTM_RECOMP_DIR.' }
$elf = "$repo\disc\SCUS_971.97"
if (-not (Test-Path $elf)) { $elf = "$runtime\discroot\SCUS_971.97" }
if (-not (Test-Path $elf)) { throw "No ELF in $repo\disc or $runtime\discroot" }

$work = Join-Path $runtime 'logs\vu0bench_windows'
New-Item -ItemType Directory -Force $work | Out-Null
Write-Host '== generate the VU0 microprograms'
# native commands with their stderr in a log: their exit code decides (Windows PowerShell 5.1 can stop on a native stderr line otherwise)
$ErrorActionPreference = 'Continue'
python "$repo\tools\vu0native.py" $elf "$work\wotm_vu0_native.gen.inc" > "$work\gen.log" 2>&1
if ($LASTEXITCODE -ne 0) { Get-Content "$work\gen.log" -Tail 20; throw 'vu0native.py failed' }

Write-Host '== build vu0bench (MSVC, the runtime''s flags that decide the float bits: /O2 /fp:precise /arch:AVX2, x64)'
$inc = @("$repo\native", "$repo\native\runtime", "$runtime\PS2Recomp\ps2xRuntime\include", $work) | ForEach-Object { "/I$_" }
& cl.exe /nologo /std:c++20 /EHsc /O2 /Ob2 /Oi /fp:precise /arch:AVX2 /DNDEBUG @inc "$repo\tools\vu0bench.cpp" "$repo\native\runtime\wotm_vu0_native.cpp" "/Fo$work\\" "/Fe$work\vu0bench.exe" > "$work\build.log" 2>&1
if ($LASTEXITCODE -ne 0) { Get-Content "$work\build.log" -Tail 30; throw "the build failed (log: $work\build.log)" }

Write-Host '== replay the recorded calls and compare with the other machine''s results'
& "$work\vu0bench.exe" (Join-Path $Kept 'vu0_code.bin') (Join-Path $Kept 'vu0_samples.bin') --expect (Join-Path $Kept 'native.results')
$status = $LASTEXITCODE
if ($status -eq 0) { Write-Host 'PASSED: MSVC x64 gives the recorded machine''s results on every call' } else { Write-Host "FAILED (vu0bench exit $status)" }
exit $status
