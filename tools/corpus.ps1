# Corpus de regressao da checagem VU1 x nativo: instantaneos (.vu1snap) nomeados pelo que mostram, fora do repo, e uma checagem
# que roda todos em segundos (tools\vu1cmp.ps1) e grava um CSV comparavel com o de antes de uma mudanca em native/.
#   tools\corpus.ps1 -Capture [-Dir <pasta>] [-Only menus,luta_centro,...]
#       grava em <runtime>\corpus (padrao): menus.vu1snap (um quadro por tela do menu; menus.txt diz g_frame=tela),
#       luta_centro.vu1snap (1 jogador + 1 IA, fase 1, g_frame 300), luta_3ia.vu1snap (3 IAs, fase 2), luta_hud.vu1snap (como luta_centro, g_frame 1200, com o HUD na tela), destrocos.vu1snap
#       (primeiro sistema de destrocos desenhado, fx:13, rajada de 6 quadros; o arco eletrico ja aparece nas lutas). Cada sessao e uma execucao do
#       runtime (build_dev) de ate -Secs segundos; a que nao disparar fica sem arquivo (o script avisa).
#   tools\corpus.ps1 -Check [-Dir <pasta>] [-Baseline <resumo.csv>] [-NoPng]
#       roda o vu1cmp em todos os .vu1snap da pasta e grava <Dir>\resumo_<data_hora>.csv (uma linha por quadro: casados, faltam,
#       sobram, textura, particulas, inventario e pixels diferentes). Com -Baseline, compara com esse CSV e lista o que piorou.
#       -NoPng pula as imagens e o diff por pixel (mais rapido).
param(
    [switch]$Capture,
    [switch]$Check,
    [string]$Dir = '',
    [string]$Only = '',
    [string]$Baseline = '',
    [switch]$NoPng,
    [int]$Secs = 90
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$runtime = if ($env:WOTM_RECOMP_DIR) { $env:WOTM_RECOMP_DIR } elseif (Test-Path "$repo\.wotm_recomp_dir") { (Get-Content "$repo\.wotm_recomp_dir" -TotalCount 1).Trim() } else { '' }
if (-not $runtime -or -not (Test-Path "$runtime\build_dev\ps2xRuntime\ps2EntryRunner.exe")) { throw "Runtime nao encontrado: defina WOTM_RECOMP_DIR ou rode build_runtime.bat." }
if (-not $Dir) { $Dir = Join-Path $runtime 'corpus' }
New-Item -ItemType Directory -Force $Dir | Out-Null
if (-not $Capture -and -not $Check) { Write-Host "Use -Capture e/ou -Check (ver o cabecalho do script)."; exit 2 }

# Uma sessao do runtime com as variaveis dadas; grava <nome>.vu1snap (+ <nome>.txt com g_frame=tela) em $Dir.
function Run-Session([string]$name, [hashtable]$vars, [int]$secs) {
    $disc = "$runtime\discroot"
    $saved = @{}
    foreach ($k in 'PS2X_PAD','PS2X_FMV','PS2X_SKIP_MOVIES','PS2X_SKIP_CMOVIE','PS2X_NO_VU1','PS2X_NO_VIF1','PS2X_NO_GS','PS2X_SCENE','PS2X_CD_IMAGE','WOTM_REPO','WOTM_MATCH','WOTM_AIVSAI','WOTM_VU1CHECK','WOTM_VU1CHECK_WHEN','WOTM_VU1CHECK_SHOTS','WOTM_VU1CHECK_SNAP','WOTM_VU1CHECK_SNAP_MAX','WOTM_VU1CHECK_BURST') {
        $saved[$k] = [Environment]::GetEnvironmentVariable($k)
        Remove-Item "Env:$k" -ErrorAction SilentlyContinue
    }
    $env:PS2X_SKIP_MOVIES = '1'; $env:PS2X_SKIP_CMOVIE = '1'
    $env:PS2X_NO_VU1 = '1'; $env:PS2X_NO_VIF1 = '1'; $env:PS2X_NO_GS = '1'; $env:PS2X_SCENE = '1'
    $env:PS2X_CD_IMAGE = "$repo\ISO\SCUS_971.97.War of the Monsters.iso"
    $env:WOTM_REPO = $repo
    $env:WOTM_SETTINGS = '0'   # sem o settings.ini do painel/launcher
    $env:WOTM_PROMPTS = 'original'   # icones 3D do jogo, como na referencia
    $env:WOTM_VU1CHECK = '1'
    $env:WOTM_VU1CHECK_SNAP = Join-Path $Dir "cap_$name"
    $env:WOTM_VU1CHECK_SNAP_MAX = '1024'
    foreach ($k in $vars.Keys) { [Environment]::SetEnvironmentVariable($k, [string]$vars[$k]) }
    Remove-Item (Join-Path $Dir "cap_$name*.vu1snap") -ErrorAction SilentlyContinue
    $err = Join-Path $Dir "$name.err.log"
    Write-Host ("[{0}] ate {1} s..." -f $name, $secs)
    $p = Start-Process -FilePath "$runtime\build_dev\ps2xRuntime\ps2EntryRunner.exe" -ArgumentList "`"$disc\SCUS_971.97`"" -WorkingDirectory $disc -NoNewWindow -PassThru `
        -RedirectStandardError $err -RedirectStandardOutput (Join-Path $Dir "$name.out.log")
    if (-not $p.WaitForExit($secs * 1000)) { Stop-Process -Id $p.Id -Force; $p.WaitForExit(15000) | Out-Null }
    # Remove-Item, nao SetEnvironmentVariable($k, $null): o PowerShell passa o $null como "" e a variavel fica definida vazia; o runtime
    # so testa getenv != nullptr, e um PS2X_NO_GS vazio desligava o GS da referencia no -Check seguinte (referencias pretas, px altos)
    foreach ($k in $saved.Keys) { if ($null -eq $saved[$k]) { Remove-Item "Env:$k" -ErrorAction SilentlyContinue } else { Set-Item "Env:$k" $saved[$k] } }
    $cap = Get-ChildItem $Dir -Filter "cap_$name*.vu1snap" | Select-Object -First 1
    if (-not $cap) { Write-Host ("[{0}] nao disparou (sem instantaneo)" -f $name) -ForegroundColor Yellow; return }
    for ($try = 0; $try -lt 20; $try++) {   # o processo morto pode segurar o arquivo por um instante
        try { Move-Item $cap.FullName (Join-Path $Dir "$name.vu1snap") -Force; break } catch { Start-Sleep -Seconds 1 }
    }
    # sidecar: g_frame=tela de cada quadro amostrado (o instantaneo so guarda o g_frame)
    $shots = Get-Content $err | ForEach-Object { if ($_ -match 'sampled frame: g_frame (\d+), currScreen (\d+)') { '{0}={1}' -f $Matches[1], $Matches[2] } }
    Set-Content (Join-Path $Dir "$name.txt") ($shots -join "`n") -Encoding ASCII
    Remove-Item (Join-Path $Dir "$name.out.log") -ErrorAction SilentlyContinue
    Write-Host ("[{0}] {1} quadro(s), {2} MB" -f $name, @($shots).Count, [int]((Get-Item (Join-Path $Dir "$name.vu1snap")).Length / 1MB))
}

if ($Capture) {
    . "$runtime\pad_script.ps1"
    $sessions = [ordered]@{
        # menus: o roteiro do run_live (X pula os filmes da abertura); o antigo -First 12 apertava Start antes da tela 20 aceitar
        menus      = @{ PS2X_PAD = (Get-PadScript -SkipFrom 3 -First 9 -Step 1.5); WOTM_VU1CHECK_WHEN = 'mem:0x6F8464' }
        luta_centro = @{ WOTM_MATCH = '1,2,1'; WOTM_AIVSAI = '1'; WOTM_VU1CHECK_WHEN = 'frame:300-100000'; WOTM_VU1CHECK_SHOTS = '1' }
        luta_3ia    = @{ WOTM_MATCH = '1,2,3,4,2'; WOTM_AIVSAI = '1'; WOTM_VU1CHECK_WHEN = 'frame:300-100000'; WOTM_VU1CHECK_SHOTS = '1' }
        luta_hud    = @{ WOTM_MATCH = '1,2,1'; WOTM_AIVSAI = '1'; WOTM_VU1CHECK_WHEN = 'frame:1200-100000'; WOTM_VU1CHECK_SHOTS = '1' }
        destrocos   = @{ WOTM_MATCH = '1,2,1'; WOTM_AIVSAI = '1'; WOTM_VU1CHECK_WHEN = 'fx:13'; WOTM_VU1CHECK_SHOTS = '1'; WOTM_VU1CHECK_BURST = '6' }
    }
    $wanted = if ($Only) { @($Only -split ',' | ForEach-Object { $_.Trim() }) } else { @($sessions.Keys) }
    foreach ($name in $sessions.Keys) {
        if ($wanted -notcontains $name) { continue }
        Run-Session $name $sessions[$name] $(if ($name -eq 'menus') { [Math]::Min($Secs, 75) } else { $Secs })
    }
}

if ($Check) {
    $files = Get-ChildItem $Dir -Filter '*.vu1snap' | Sort-Object Name
    if (-not $files) { throw "Nenhum .vu1snap em $Dir" }
    # Uma execucao do runtime para o corpus inteiro (antes era um powershell + um runtime por arquivo, e abrir custava mais que comparar);
    # a saida de cada arquivo comeca em "[vu1cmp] arquivo <caminho>"
    $cmpArgs = '--vu1cmp ' + (($files | ForEach-Object { "`"$($_.FullName)`"" }) -join ' ')
    if ($NoPng) { $cmpArgs += ' --nopng' }
    $cmpErr = Join-Path $Dir 'check.err.log'
    Write-Host ("vu1cmp em {0} arquivo(s)..." -f @($files).Count)
    $env:WOTM_SETTINGS = '0'; $env:WOTM_PROMPTS = 'original'   # sem o settings.ini e com os icones 3D do jogo, como na referencia
    Start-Process -FilePath "$runtime\build_dev\ps2xRuntime\ps2EntryRunner.exe" -ArgumentList $cmpArgs -WorkingDirectory "$runtime\discroot" -NoNewWindow -Wait `
        -RedirectStandardError $cmpErr -RedirectStandardOutput (Join-Path $Dir 'check.out.log') | Out-Null
    $byFile = @{}
    $curList = $null
    foreach ($l in (Get-Content $cmpErr)) {
        if ($l -match '^\[vu1cmp\] file (.+)$') { $curList = New-Object System.Collections.Generic.List[string]; $byFile[$Matches[1]] = $curList; continue }
        if ($null -ne $curList) { $curList.Add(($l -replace '^\[vu1check\] ', '')) }
    }
    $rows = @()
    foreach ($f in $files) {
        $name = $f.BaseName
        $screens = @{}
        if (Test-Path (Join-Path $Dir "$name.txt")) { Get-Content (Join-Path $Dir "$name.txt") | ForEach-Object { if ($_ -match '^(\d+)=(\d+)$') { $screens[$Matches[1]] = $Matches[2] } } }
        $lines = if ($byFile.ContainsKey($f.FullName)) { $byFile[$f.FullName] } else { @() }
        if (-not ($lines -match ': [1-9]\d* frame\(s\) compared$')) {
            Write-Host ("{0}: falhou" -f $name) -ForegroundColor Yellow
            $lines | Where-Object { $_ -match '\[vu1cmp\]' } | ForEach-Object { Write-Host "  $_" -ForegroundColor Yellow }
        }
        $cur = $null
        foreach ($l in $lines) {
            if ($l -match '^g=(\d+) objects .* matched ([\d.]+)% missing (\d+) extra (\d+) .* texture differs (\d+)') {
                $cur = [ordered]@{ arquivo = $name; quadro = [int]$Matches[1]; tela = $screens[$Matches[1]]; casados = [double]$Matches[2]; faltam = [int]$Matches[3]; sobram = [int]$Matches[4]; textura = [int]$Matches[5]; fx_casados = ''; inv_ref = ''; inv_nat = ''; px = '' }
                $rows += [pscustomobject]$cur
            } elseif ($cur -and $l -match '^g=\d+ particles: .* quads vu (\d+) .* matched ([\d.]+)%') { if ([int]$Matches[1] -gt 0) { $rows[-1].fx_casados = [double]$Matches[2] } }
            elseif ($cur -and $l -match '^inv g=\d+ reference only: \d+ groups, (\d+) primitives .* native only: (\d+) objects, (\d+) tris') { $rows[-1].inv_ref = [int]$Matches[1]; $rows[-1].inv_nat = [int]$Matches[3] }
            elseif ($cur -and $l -match 'differing pixels (\d+)') { $rows[-1].px = [int]$Matches[1] }
        }
        Write-Host ("{0}: {1} quadro(s)" -f $name, @($rows | Where-Object { $_.arquivo -eq $name }).Count)
    }
    $out = Join-Path $Dir ('resumo_' + (Get-Date -Format 'yyyy-MM-dd_HHmmss') + '.csv')
    $rows | Export-Csv $out -NoTypeInformation -Encoding ASCII
    Write-Host "resumo: $out"
    $rows | Format-Table arquivo, quadro, tela, casados, faltam, sobram, textura, fx_casados, inv_ref, inv_nat, px -AutoSize | Out-String -Width 200 | Write-Host
    if ($Baseline) {
        $base = @{}
        Import-Csv $Baseline | ForEach-Object { $base[$_.arquivo + '#' + $_.quadro] = $_ }
        # O CSV sai com a cultura do Windows (pt-BR: "99,9"); [double]"99,9" daria 999. Num le os dois formatos.
        function Num($v) { [double]::Parse(([string]$v).Replace(",", "."), [Globalization.CultureInfo]::InvariantCulture) }
        $worse = 0
        foreach ($r in $rows) {
            $b = $base[$r.arquivo + '#' + $r.quadro]
            if (-not $b) { Write-Host ("{0} g={1}: novo (sem base)" -f $r.arquivo, $r.quadro); continue }
            $msgs = @()
            if ((Num $r.casados) -lt (Num $b.casados) - 0.1) { $msgs += ('casados {0}% -> {1}%' -f $b.casados, $r.casados) }
            foreach ($k in 'faltam','sobram','textura','inv_ref','inv_nat','px') {
                if ($r.$k -ne '' -and $b.$k -ne '' -and (Num $r.$k) -gt (Num $b.$k) * 1.05 + 2) { $msgs += ('{0} {1} -> {2}' -f $k, $b.$k, $r.$k) }
            }
            if ($r.fx_casados -ne '' -and $b.fx_casados -ne '' -and (Num $r.fx_casados) -lt (Num $b.fx_casados) - 0.1) { $msgs += ('fx_casados {0}% -> {1}%' -f $b.fx_casados, $r.fx_casados) }
            if ($msgs) { $worse++; Write-Host ("PIOROU {0} g={1} (tela {2}): {3}" -f $r.arquivo, $r.quadro, $r.tela, ($msgs -join '; ')) -ForegroundColor Yellow }
        }
        if (-not $worse) { Write-Host "Nada piorou em relacao a $Baseline" -ForegroundColor Green }
    }
}
