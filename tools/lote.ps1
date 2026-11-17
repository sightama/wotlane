# Batch of AI-vs-AI fights in the runtime (WOTM_MATCH + WOTM_AIVSAI), to find bugs with nobody watching.
# Each combination (player monster x first AI x stage) runs for -Secs seconds in its own process: the fight is endless
# (m_killTarget 0, the knocked-out monster gets up on its own) and each round shows up in the log as [match]. The AI count cycles
# through -AIs over the batch (combination n uses element n mod length); the AIs after the first are drawn from -Opponents with
# seed -Seed, so the same command repeats the same batch. The runtime watchdog writes [watchdog] FROZE (with
# ram_/vram_vigia_*.bin) and [watchdog] SLOW; a crash shows up as [CRASH] or as the process ending early.
# Every -Check seconds the runtime compares, on one frame, what the VU1 microcode would draw with what the native layer draws,
# per object and in numbers (WOTM_VU1CHECK, [vu1check] lines; see docs/passos/NATIVO.md).
# Output in <runtime>\lote\<date_time>\: <player>_<AIs joined by +>_<stage>.log ([match]/[watchdog]/[CRASH]/[vu1check] lines and
# the stage/monsters loaded), .png (the screen ~12 s before the end), the watchdog dumps and resumo.csv.
#
# Usage: powershell -File tools\lote.ps1 [-Monsters 1,2,3] [-Opponents 1,2,3] [-Levels 1,2] [-AIs 1,2,3] [-Seed 1] [-Secs 180] [-Max 0] [-Check 1]
#   Indices: monsters and stages as in GLOSSARIO.md (MonsterLongNames, GameLevelNames). Without -Opponents, uses the -Monsters list.
#   -AIs: AI counts (1 to 7) to alternate between. -Max N stops after N combinations (0 = all). -Check 0 turns the comparison off.
#   The lists are comma-separated text (with -File, PowerShell does not turn "1,2" into a list).
param(
    [string]$Monsters = '1,2,3,4,5,7,8,9,10,11',
    [string]$Opponents = '',
    [string]$Levels = '1,2,4,5,7,8,9,10,13,14,15,16',
    [string]$AIs = '1,2,3',
    [int]$Seed = 1,
    [int]$Secs = 180,
    [int]$Max = 0,
    [int]$Check = 1,
    [string]$Build = 'build_dev'   # build_dev_nm = guinea-pig runtime with the rewritten functions (build_runtime.bat nm); run the same batch on both and compare resumo.csv
)
$ErrorActionPreference = 'Stop'
function Split-Ints([string]$s) { @($s -split ',' | Where-Object { $_.Trim() } | ForEach-Object { [int]$_.Trim() }) }
$monList = Split-Ints $Monsters
$oppList = Split-Ints $Opponents
$levList = Split-Ints $Levels
$aiCounts = Split-Ints $AIs
if (@($aiCounts | Where-Object { $_ -lt 1 -or $_ -gt 7 }).Count) { throw "-AIs accepts 1 to 7." }
$repo = Split-Path $PSScriptRoot -Parent
$runtime = if ($env:WOTM_RECOMP_DIR) { $env:WOTM_RECOMP_DIR } elseif (Test-Path "$repo\.wotm_recomp_dir") { (Get-Content "$repo\.wotm_recomp_dir" -TotalCount 1).Trim() } else { '' }
if (-not $runtime -or -not (Test-Path "$runtime\run_live.ps1")) { throw "Runtime folder not found: run run_live.bat once or set WOTM_RECOMP_DIR." }
if ($oppList.Count -eq 0) { $oppList = $monList }

$out = Join-Path $runtime ("lote\" + (Get-Date -Format 'yyyy-MM-dd_HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$disc = "$runtime\discroot"
$summary = Join-Path $out 'resumo.csv'
'jogador,ias,fase,segundos,rodadas,vitorias,congelou,lento,crash,saiu_antes,checagens,casados_pior,objetos_divergentes,textura_difere_pior,objetos_textura,fx_casados_pior,fx_tipos_divergentes,vif1_fora_de_passo,uv_difere_pior,cor_difere_pior,alfa_difere_pior,abe_difere_pior,alpha_difere_pior' | Set-Content $summary -Encoding ASCII

# Each combination: @(player, ai1, [ai2, [ai3,]] stage)
$rng = New-Object System.Random $Seed
$combos = New-Object System.Collections.Generic.List[int[]]
foreach ($l in $levList) {
    foreach ($a in $monList) {
        foreach ($b in $oppList) {
            $k = $aiCounts[$combos.Count % $aiCounts.Count]
            $c = @($a, $b)
            for ($i = 1; $i -lt $k; $i++) { $c += $oppList[$rng.Next($oppList.Count)] }
            $combos.Add([int[]]($c + $l))
        }
    }
}
if ($Max -gt 0 -and $combos.Count -gt $Max) { $combos = $combos.GetRange(0, $Max) }
$total = $combos.Count
Write-Host "Batch: $total combinations of $Secs s (~$([math]::Ceiling($total * ($Secs + 10) / 60)) min). Output: $out"

$env:WOTM_REPO = $repo
$env:WOTM_AIVSAI = '1'
$env:WOTM_SETTINGS = '0'   # without the panel/launcher settings.ini: same batch on every machine
if ($Check -gt 0) { $env:WOTM_VU1CHECK = [string]$Check } else { Remove-Item Env:WOTM_VU1CHECK -ErrorAction SilentlyContinue }
$n = 0
foreach ($c in $combos) {
    $n++
    $player = $c[0]; $level = $c[-1]
    $foes = $c[1..($c.Count - 2)]
    $tag = '{0}_{1}_{2}' -f $player, ($foes -join '+'), $level
    $env:WOTM_MATCH = $c -join ','
    Write-Host ("[{0}/{1}] player {2} vs {3}, stage {4}" -f $n, $total, $player, ($foes -join ' '), $level)
    Get-Process ps2EntryRunner, wotm -ErrorAction SilentlyContinue | Stop-Process -Force
    Get-ChildItem $disc -Filter '*_vigia_*.bin' -ErrorAction SilentlyContinue | Remove-Item -Force
    Get-ChildItem $disc -Filter 'shot_*.png' -ErrorAction SilentlyContinue | Remove-Item -Force
    $env:PS2X_SHOT_FRAMES = [string][math]::Max(60, ($Secs - 12) * 60)   # one screenshot near the end (screen frames, ~60/s)
    $start = Get-Date
    Start-Process powershell -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$runtime\run_live.ps1`" -Auto 0 -Build $Build" -WindowStyle Minimized | Out-Null
    Start-Sleep -Seconds 5
    $early = $false
    while (((Get-Date) - $start).TotalSeconds -lt $Secs) {
        if (-not (Get-Process ps2EntryRunner, wotm -ErrorAction SilentlyContinue)) { $early = $true; break }
        Start-Sleep -Seconds 2
    }
    $elapsed = [int]((Get-Date) - $start).TotalSeconds
    Get-Process ps2EntryRunner, wotm -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 2

    $liveLog = if (Test-Path "$runtime\logs\live.err.log") { "$runtime\logs\live.err.log" } else { "$runtime\live.err.log" }   # older run_live.ps1 writes it to the root
    $lines = @(Select-String -Path $liveLog -Pattern '\[match\]|\[wotm\] stage load|\[watchdog\]|\[CRASH\]|\[native\] resources|\[vu1check\]|\[flare\]' | ForEach-Object { $_.Line })
    $lines | Set-Content (Join-Path $out "$tag.log") -Encoding UTF8
    Get-ChildItem $disc -Filter '*_vigia_*.bin' -ErrorAction SilentlyContinue | ForEach-Object { Move-Item $_.FullName (Join-Path $out ("{0}_{1}" -f $tag, $_.Name)) -Force }
    Get-ChildItem $disc -Filter 'shot_*.png' -ErrorAction SilentlyContinue | Select-Object -First 1 | ForEach-Object { Move-Item $_.FullName (Join-Path $out "$tag.png") -Force }

    $rounds = @($lines | Where-Object { $_ -match 'round \d+ ended' })
    $wins = ''   # each monster's wins in the last round, player first, separated by /
    if ($rounds.Count -gt 0) { $wins = (@([regex]::Matches($rounds[-1], 'wins (-?\d+);') | ForEach-Object { $_.Groups[1].Value }) -join '/') }
    $frozen = @($lines | Where-Object { $_ -match 'FROZE' }).Count
    $slow = @($lines | Where-Object { $_ -match 'SLOW' }).Count
    $crash = @($lines | Where-Object { $_ -match '\[CRASH\]' }).Count
    # VU1 vs native comparison: how many checks, the worst % of VU1 triangles matched and how many objects (polyPkt) showed a difference
    $checks = @($lines | ForEach-Object { if ($_ -match '\[vu1check\] g=\d+ objects .* matched ([\d.]+)%') { [double]$Matches[1] } })
    $worst = if ($checks.Count) { ($checks | Measure-Object -Minimum).Minimum.ToString([cultureinfo]::InvariantCulture) } else { '' }   # decimal point (CSV)
    $diverging = @($lines | ForEach-Object { if ($_ -match '\[vu1check\]\s+pkt ([0-9a-f]+)') { $Matches[1] } } | Sort-Object -Unique).Count
    # texture: the worst count, in one frame, of matched triangles whose TEX0 differs from native, and how many objects had a different texture
    $texDiff = @($lines | ForEach-Object { if ($_ -match '\[vu1check\] g=\d+ .* texture differs (\d+)') { [int]$Matches[1] } })
    $texWorst = if ($texDiff.Count) { ($texDiff | Measure-Object -Maximum).Maximum } else { '' }
    $texObjs = @($lines | ForEach-Object { if ($_ -match '\[vu1check\]\s+texture pkt ([0-9a-f]+)') { $Matches[1] } } | Sort-Object -Unique).Count
    # particles: the worst % of VU1 quads matched in one frame (only frames with particles) and the system types that diverged
    $fx = @($lines | ForEach-Object { if ($_ -match '\[vu1check\] g=\d+ particles: .* quads vu (\d+) .* matched ([\d.]+)%') { if ([int]$Matches[1] -gt 0) { [double]$Matches[2] } } })
    $fxWorst = if ($fx.Count) { ($fx | Measure-Object -Minimum).Minimum.ToString([cultureinfo]::InvariantCulture) } else { '' }
    $fxTypes = (@($lines | ForEach-Object { if ($_ -match '\[vu1check\]\s+fx \d+ preset \d+ type (\d+)') { $Matches[1] } } | Sort-Object { [int]$_ } -Unique) -join '/')
    # invalid VIF1 codes since boot (VIF1 fell out of step and read data as commands; lead 5 in VU1CHECK.md): the highest count seen
    $vifBad = @($lines | ForEach-Object { if ($_ -match '\[vu1check\] g=\d+ objects .* VIF1 out of step (\d+)') { [int]$Matches[1] } })
    $vifBadMax = if ($vifBad.Count) { ($vifBad | Measure-Object -Maximum).Maximum } else { '' }
    # attributes of matched triangles (uv, color, alpha, ABE, ALPHA): the worst count of differing triangles in one frame
    $attrWorst = foreach ($name in 'uv differs', 'color differs', 'vertex alpha differs', 'ABE differs', 'ALPHA differs') {
        $v = @($lines | ForEach-Object { if ($_ -match ('\[vu1check\] g=\d+ attributes: .*\| ' + $name + ' (\d+)')) { [int]$Matches[1] } })
        if ($v.Count) { ($v | Measure-Object -Maximum).Maximum } else { '' }
    }
    '{0},{1},{2},{3},{4},{5},{6},{7},{8},{9},{10},{11},{12},{13},{14},{15},{16},{17},{18},{19},{20},{21},{22}' -f $player, ($foes -join '+'), $level, $elapsed, $rounds.Count, $wins, $frozen, $slow, $crash,
        [int]$early, $checks.Count, $worst, $diverging, $texWorst, $texObjs, $fxWorst, $fxTypes, $vifBadMax, $attrWorst[0], $attrWorst[1], $attrWorst[2], $attrWorst[3], $attrWorst[4] | Add-Content $summary -Encoding ASCII
    if ($frozen -or $crash -or $early) { Write-Host ("   PROBLEM: froze={0} crash={1} exited_early={2}" -f $frozen, $crash, [int]$early) -ForegroundColor Red }
    else { Write-Host ("   ok: {0} rounds, slow={1}" -f $rounds.Count, $slow) }
}
Remove-Item Env:WOTM_MATCH, Env:WOTM_AIVSAI, Env:WOTM_VU1CHECK, Env:PS2X_SHOT_FRAMES -ErrorAction SilentlyContinue
Write-Host "Summary: $summary"
