# Partida automatica em que o monstro anda e ataca (soco/chute) depois de chegar na fase: gera eventos de destruicao para testar o desenho nativo.
# Imprime [destr] (WOTM_DESTRSTAT) e [cam] (WOTM_CAMLOG) em logs\atk.err.log, na pasta do runtime. -Dump "3900,4000" grava ram_<n>.bin/vram_<n>.bin em discroot
# (quadros de apresentacao; renderize com build\viewer.exe ram_<n>.bin --disc disc). Uso: powershell -File tools\live_attack.ps1 [-Seconds 140] [-Dump "..."]
param([int]$Seconds = 130, [string]$Dump = "")
$repo = Split-Path $PSScriptRoot -Parent
$root = if ($env:WOTM_RECOMP_DIR) { $env:WOTM_RECOMP_DIR } elseif (Test-Path "$repo\.wotm_recomp_dir") { (Get-Content "$repo\.wotm_recomp_dir" -TotalCount 1).Trim() } else { Join-Path (Split-Path $repo -Parent) "wotm-recomp-win" }
$dr = "$root\discroot"
$env:PS2X_SKIP_MOVIES = "1"; $env:PS2X_SKIP_CMOVIE = "1"
$env:PS2X_NO_VU1 = "1"; $env:PS2X_NO_VIF1 = "1"; $env:PS2X_NO_GS = "1"; $env:PS2X_SCENE = "1"
. "$root\pad_script.ps1"
$base = Get-PadScript
$extra = @()
$t = 62.0
for ($i = 0; $i -lt 70; $i++) {
    $dir = @("up","left","down","right")[$i % 4]
    $extra += ("{0}:0.5:{1}" -f $t, $dir); $t += 0.6
    foreach ($k in 0..2) { $extra += ("{0}:0.15:square" -f $t); $t += 0.35 }
    $extra += ("{0}:0.2:triangle" -f $t); $t += 0.5
}
$env:PS2X_PAD = $base + ";" + ($extra -join ";")
$env:WOTM_DESTRSTAT = "1"; $env:WOTM_CAMLOG = "1"
if ($Dump) { $env:PS2X_DUMP_RAM = $Dump; $env:PS2X_GS_UPLOADS = "1" }   # uploads do GS ligados: a VRAM do dump guarda as texturas dos efeitos
$env:PS2X_CD_IMAGE = "$repo\ISO\SCUS_971.97.War of the Monsters.iso"
Set-Location $dr
New-Item -ItemType Directory -Force "$root\logs" | Out-Null
$p = Start-Process -FilePath "$root\build_dev\ps2xRuntime\ps2EntryRunner.exe" -ArgumentList "`"$dr\SCUS_971.97`"" -WorkingDirectory $dr -PassThru -RedirectStandardOutput "$root\logs\atk.out.log" -RedirectStandardError "$root\logs\atk.err.log"
Start-Sleep -Seconds $Seconds
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
"fim"
