# Checagem VU1 x nativo sem jogo, a partir de instantaneos (.vu1snap) gravados pelo runtime com WOTM_VU1CHECK_SNAP=<prefixo>.
# Roda `ps2EntryRunner --vu1cmp` (wotm_vu1check.inc) para cada arquivo e imprime as linhas [vu1check]; leva ~1 s por instantaneo.
#   tools\vu1cmp.ps1 <arquivo.vu1snap | pasta> [-Frame <g_frame>] [-Dump <prefixo>] [-Full] [-Build build_dev]
#   Um arquivo guarda os quadros de uma sessao (gatilho/rajada: WOTM_VU1CHECK_WHEN, WOTM_VU1CHECK_BURST); todos sao comparados.
#   -Frame <g_frame>: so esse quadro do arquivo
#   -Full: todas as linhas [vu1check] (padrao: so as linhas de resumo de cada quadro: objetos, atributos, particulas, sprites)
#   -Dump <prefixo>: grava os triangulos de cada checagem em <prefixo>_<g_frame>.csv (o mesmo WOTM_VU1CHECK_DUMP)
# A pasta do runtime vem de WOTM_RECOMP_DIR ou de .wotm_recomp_dir na raiz do repo (gravado pelo build_runtime.bat).
param(
    [Parameter(Mandatory = $true)][string]$Path,
    [int]$Frame = 0,
    [string]$Dump = '',
    [switch]$Full,
    [string]$Build = 'build_dev'
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$runtime = if ($env:WOTM_RECOMP_DIR) { $env:WOTM_RECOMP_DIR } elseif (Test-Path "$repo\.wotm_recomp_dir") { (Get-Content "$repo\.wotm_recomp_dir" -TotalCount 1).Trim() } else { '' }
if (-not $runtime -or -not (Test-Path "$runtime\$Build\ps2xRuntime\ps2EntryRunner.exe")) {
    Write-Error "Nao achei o runtime ($Build\ps2xRuntime\ps2EntryRunner.exe) em '$runtime': defina WOTM_RECOMP_DIR ou rode build_runtime.bat uma vez."
}
$exe = "$runtime\$Build\ps2xRuntime\ps2EntryRunner.exe"
$files = if (Test-Path $Path -PathType Container) { Get-ChildItem $Path -Filter '*.vu1snap' | Sort-Object { [int](($_.BaseName -split '_')[-1]) } | ForEach-Object { $_.FullName } } else { @((Resolve-Path $Path).Path) }
if (-not $files) { Write-Error "Nenhum .vu1snap em $Path" }
$err = Join-Path $env:TEMP 'wotm_vu1cmp.err.log'
$out = Join-Path $env:TEMP 'wotm_vu1cmp.out.log'
$rc = 0
foreach ($f in $files) {
    $args = "--vu1cmp `"$f`""
    if ($Frame -gt 0) { $args += " --frame $Frame" }
    if ($Dump) { $args += " --dump `"$Dump`"" }
    # Start-Process redireciona sem passar pelo PowerShell (o `2>` do PS 5 grava UTF-16 e quebra as linhas longas)
    $p = Start-Process -FilePath $exe -ArgumentList $args -WorkingDirectory "$runtime\discroot" -NoNewWindow -Wait -PassThru -RedirectStandardError $err -RedirectStandardOutput $out
    $lines = Get-Content $err
    Write-Host ("== {0}" -f (Split-Path $f -Leaf)) -ForegroundColor Cyan
    if ($p.ExitCode -ne 0) {
        $rc = 1
        $lines | Where-Object { $_ -match '\[vu1cmp\]' } | ForEach-Object { Write-Host $_ -ForegroundColor Yellow }
        continue
    }
    $sel = if ($Full) { $lines | Where-Object { $_ -match '\[vu1check\]' } } else { $lines | Where-Object { $_ -match '\[vu1check\] g=\d+ ' } }
    $sel | ForEach-Object { $_ -replace '^\[vu1check\] ', '' }
}
exit $rc
