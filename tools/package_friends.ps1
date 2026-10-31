# ABOUTME: Builds the package for friends who own the disc: launcher, wotm.exe with its DLLs, wotm_server, the repo's mods and a setup
# ABOUTME: that extracts the ELF from each one's ISO. No disc file goes in. Output: dist\wotlane-<buildid>\ and .zip (dist\ not in git).
#
# Usage: powershell -File tools\package_friends.ps1. Prints the package folder path on the last line. Test: tools\package_check.ps1.
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$runtime = (Get-Content (Join-Path $repo '.wotm_recomp_dir') -TotalCount 1).Trim()
$bin = Join-Path $runtime 'build_dev\ps2xRuntime'
foreach ($f in 'wotm.exe', 'wotm.buildid') { if (-not (Test-Path (Join-Path $bin $f))) { throw "missing $bin\$f (build_runtime.bat)" } }
$id = (Get-Content (Join-Path $bin 'wotm.buildid')).Trim()
# the exe must be built from the current code: the server rejects different builds, and the package would be from another commit
$now = (& python (Join-Path $PSScriptRoot 'build_id.py') | Select-Object -Last 1).Trim()
if ($now -ne $id) { throw "wotm.exe is from other code ($id; current is $now): run build_runtime.bat" }

& cmd /c "`"$repo\tools\build_net.bat`" notest" | Out-Null
$server = Join-Path $repo 'native\net\build\wotm_server.exe'
if (-not (Test-Path $server)) { throw 'wotm_server.exe did not build (tools\build_net.bat)' }
& cmd /c "`"$repo\tools\launcher\build.bat`""
if ($LASTEXITCODE -ne 0) { throw 'the launcher did not build (tools\launcher\build.bat)' }

# MSVC DLLs (the exe and the server use the dynamic CRT): from the Redist folder of the same compiler version
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$crt = $null
foreach ($vs in & $vswhere -all -products * -property installationPath) {
    $crt = Get-ChildItem "$vs\VC\Redist\MSVC\14.44.*\x64\Microsoft.VC143.CRT" -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($crt) { break }
}
if (-not $crt) { throw 'MSVC 14.44 Redist (Microsoft.VC143.CRT x64) not found' }

$out = Join-Path $repo "dist\wotlane-$id"
if (Test-Path $out) { Remove-Item $out -Recurse -Force }
$pbin = Join-Path $out 'runtime\build_dev\ps2xRuntime'
New-Item -ItemType Directory -Force $pbin, (Join-Path $out 'native\net\build'), (Join-Path $out 'tools'), (Join-Path $out 'ISO'),
    (Join-Path $out 'mods') | Out-Null
Copy-Item (Join-Path $bin 'wotm.exe'), (Join-Path $bin 'wotm.buildid') $pbin
Copy-Item (Join-Path $bin '*.dll') $pbin
Copy-Item (Join-Path $crt.FullName '*.dll') $pbin
Copy-Item (Join-Path $crt.FullName '*.dll') (Join-Path $out 'native\net\build')
Copy-Item $server (Join-Path $out 'native\net\build')
Copy-Item (Join-Path $repo 'WotM_Launcher.exe') $out
Copy-Item (Join-Path $repo 'tools\sync_mods.ps1') (Join-Path $out 'tools')
Copy-Item (Join-Path $repo 'tools\runtime_template\run_live.ps1') (Join-Path $out 'runtime\run_live.template.ps1')
Copy-Item (Join-Path $PSScriptRoot 'friends\setup.ps1'), (Join-Path $PSScriptRoot 'friends\Setup.bat'), (Join-Path $PSScriptRoot 'friends\README.txt') $out
foreach ($m in Get-ChildItem (Join-Path $repo 'mods') -Directory) {
    if ($m.Name.StartsWith('_') -or $m.Name.StartsWith('.')) { continue }   # disabled (same rule as tools\sync_mods.ps1)
    Copy-Item $m.FullName (Join-Path $out 'mods') -Recurse
}
Set-Content (Join-Path $out 'ISO\PUT_YOUR_ISO_HERE.txt') 'Put your own War of the Monsters (USA) .iso in this folder, then run Setup.bat.' -Encoding ascii
Set-Content (Join-Path $out 'BUILD.txt') "wotlane build $id, commit $(git -C $repo rev-parse --short HEAD)" -Encoding ascii

$bad = @(Get-ChildItem $out -Recurse -File | Where-Object { $_.Name -match '^SCUS|\.(iso|elf|rom)$' })
if ($bad.Count) { throw "disc file in the package: $($bad.FullName -join ', ')" }
$zip = "$out.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path $out -DestinationPath $zip -CompressionLevel Optimal
Write-Host "package: $zip ($([math]::Round((Get-Item $zip).Length / 1MB)) MB)"
$out
