# Lote de lutas IA contra IA no runtime (WOTM_MATCH + WOTM_AIVSAI), para achar bugs sem ninguem olhando.
# Cada combinacao (monstro do jogador x monstro da IA x fase) roda por -Secs segundos numa execucao propria: a luta e continua
# (m_killTarget 0, o derrubado volta sozinho) e cada rodada sai no log como [partida]. O vigia do runtime grava [vigia] CONGELOU
# (com ram_/vram_vigia_*.bin) e [vigia] LENTO; uma queda sai como [CRASH] ou como o processo terminando antes da hora.
# Saida em <runtime>\lote\<data_hora>\: <jogador>_<ia>_<fase>.log (linhas [partida]/[vigia]/[CRASH] e a fase/monstros carregados),
# <jogador>_<ia>_<fase>.png (a tela ~12 s antes do fim), os dumps do vigia e resumo.csv.
#
# Uso: powershell -File tools\lote.ps1 [-Monsters 1,2,3] [-Opponents 1,2,3] [-Levels 1,2] [-Secs 180] [-Max 0]
#   Indices: monstros e fases como no GLOSSARIO.md (MonsterLongNames, GameLevelNames). Sem -Opponents, usa a mesma lista de -Monsters.
#   -Max N para depois de N combinacoes (0 = todas).
#   As listas sao texto separado por virgula (com -File o PowerShell nao converte "1,2" em lista).
param(
    [string]$Monsters = '1,2,3,4,5,7,8,9,10,11',
    [string]$Opponents = '',
    [string]$Levels = '1,2,4,5,7,8,9,10,13,14,15,16',
    [int]$Secs = 180,
    [int]$Max = 0
)
$ErrorActionPreference = 'Stop'
function Split-Ints([string]$s) { @($s -split ',' | Where-Object { $_.Trim() } | ForEach-Object { [int]$_.Trim() }) }
$monList = Split-Ints $Monsters
$oppList = Split-Ints $Opponents
$levList = Split-Ints $Levels
$repo = Split-Path $PSScriptRoot -Parent
$runtime = if ($env:WOTM_RECOMP_DIR) { $env:WOTM_RECOMP_DIR } elseif (Test-Path "$repo\.wotm_recomp_dir") { (Get-Content "$repo\.wotm_recomp_dir" -TotalCount 1).Trim() } else { '' }
if (-not $runtime -or -not (Test-Path "$runtime\run_live.ps1")) { throw "Pasta do runtime nao encontrada: rode run_live.bat uma vez ou defina WOTM_RECOMP_DIR." }
if ($oppList.Count -eq 0) { $oppList = $monList }

$out = Join-Path $runtime ("lote\" + (Get-Date -Format 'yyyy-MM-dd_HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$disc = "$runtime\discroot"
$summary = Join-Path $out 'resumo.csv'
'jogador,ia,fase,segundos,rodadas,vitorias_jogador,vitorias_ia,congelou,lento,crash,saiu_antes' | Set-Content $summary -Encoding ASCII

$combos = New-Object System.Collections.Generic.List[int[]]
foreach ($l in $levList) { foreach ($a in $monList) { foreach ($b in $oppList) { $combos.Add([int[]]@($a, $b, $l)) } } }
if ($Max -gt 0 -and $combos.Count -gt $Max) { $combos = $combos.GetRange(0, $Max) }
$total = $combos.Count
Write-Host "Lote: $total combinacoes de $Secs s (~$([math]::Ceiling($total * ($Secs + 10) / 60)) min). Saida: $out"

$env:WOTM_REPO = $repo
$env:WOTM_AIVSAI = '1'
$n = 0
foreach ($c in $combos) {
    $n++
    $tag = '{0}_{1}_{2}' -f $c[0], $c[1], $c[2]
    $env:WOTM_MATCH = '{0},{1},{2}' -f $c[0], $c[1], $c[2]
    Write-Host ("[{0}/{1}] jogador {2} contra {3}, fase {4}" -f $n, $total, $c[0], $c[1], $c[2])
    Get-Process ps2EntryRunner -ErrorAction SilentlyContinue | Stop-Process -Force
    Get-ChildItem $disc -Filter '*_vigia_*.bin' -ErrorAction SilentlyContinue | Remove-Item -Force
    Get-ChildItem $disc -Filter 'shot_*.png' -ErrorAction SilentlyContinue | Remove-Item -Force
    $env:PS2X_SHOT_FRAMES = [string][math]::Max(60, ($Secs - 12) * 60)   # uma captura perto do fim (quadros da tela, ~60/s)
    $start = Get-Date
    Start-Process powershell -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$runtime\run_live.ps1`" -Auto 0" -WindowStyle Minimized | Out-Null
    Start-Sleep -Seconds 5
    $early = $false
    while (((Get-Date) - $start).TotalSeconds -lt $Secs) {
        if (-not (Get-Process ps2EntryRunner -ErrorAction SilentlyContinue)) { $early = $true; break }
        Start-Sleep -Seconds 2
    }
    $elapsed = [int]((Get-Date) - $start).TotalSeconds
    Get-Process ps2EntryRunner -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 2

    $lines = @(Select-String -Path "$runtime\live.err.log" -Pattern '\[partida\]|\[vigia\]|\[CRASH\]|\[native\] recursos' | ForEach-Object { $_.Line })
    $lines | Set-Content (Join-Path $out "$tag.log") -Encoding UTF8
    Get-ChildItem $disc -Filter '*_vigia_*.bin' -ErrorAction SilentlyContinue | ForEach-Object { Move-Item $_.FullName (Join-Path $out ("{0}_{1}" -f $tag, $_.Name)) -Force }
    Get-ChildItem $disc -Filter 'shot_*.png' -ErrorAction SilentlyContinue | Select-Object -First 1 | ForEach-Object { Move-Item $_.FullName (Join-Path $out "$tag.png") -Force }

    $rounds = @($lines | Where-Object { $_ -match 'rodada \d+ terminou' })
    $winsP = 0; $winsA = 0
    if ($rounds.Count -gt 0 -and $rounds[-1] -match 'vitorias (-?\d+);.*vitorias (-?\d+);') { $winsP = [int]$Matches[1]; $winsA = [int]$Matches[2] }
    $frozen = @($lines | Where-Object { $_ -match 'CONGELOU' }).Count
    $slow = @($lines | Where-Object { $_ -match 'LENTO' }).Count
    $crash = @($lines | Where-Object { $_ -match '\[CRASH\]' }).Count
    '{0},{1},{2},{3},{4},{5},{6},{7},{8},{9},{10}' -f $c[0], $c[1], $c[2], $elapsed, $rounds.Count, $winsP, $winsA, $frozen, $slow, $crash, [int]$early |
        Add-Content $summary -Encoding ASCII
    if ($frozen -or $crash -or $early) { Write-Host ("   PROBLEMA: congelou={0} crash={1} saiu_antes={2}" -f $frozen, $crash, [int]$early) -ForegroundColor Red }
    else { Write-Host ("   ok: {0} rodadas, lento={1}" -f $rounds.Count, $slow) }
}
Remove-Item Env:WOTM_MATCH, Env:WOTM_AIVSAI, Env:PS2X_SHOT_FRAMES -ErrorAction SilentlyContinue
Write-Host "Resumo: $summary"
