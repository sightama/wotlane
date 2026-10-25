# ABOUTME: Monta tudo o que o port precisa a partir da sua ISO, num comando: disco extraido, PS2Recomp com os patches, jogo traduzido
# ABOUTME: (recomp\retail), pasta do runtime (runtime\) com os scripts, e o primeiro build do wotm.exe. Pode rodar de novo: pula o que ja existe.
#
# Uso: powershell -ExecutionPolicy Bypass -File tools\setup_runtime.ps1 [-Iso <caminho da .iso>] [-SkipBuild]
#   Sem -Iso usa a primeira .iso em ISO\. Precisa de: git, Python 3, CMake 3.24 ou mais novo e o Visual Studio 2022 (ou Build Tools)
#   com o compilador C++ 14.40 ou mais novo (o 14.32 tem um erro interno com o codigo traduzido). Nada do jogo vai para o repo:
#   disc\, recomp\ e runtime\ estao no .gitignore.
param([string]$Iso = '', [switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$rt = Join-Path $repo 'runtime'
$elfSha1 = '1ed7f8457436a9170d8f85386e267369c7fcdd4f'   # SCUS_971.97 do disco NTSC-U (SCUS-97197)
$ps2recompCommit = '2c5fbb9389e11dd95693385969490c9e8e6f57b4'   # base do tools\runtime_patch\wotm-runtime.patch

function Step([string]$m) { Write-Host "== $m" -ForegroundColor Cyan }
function Fail([string]$m) { Write-Host $m -ForegroundColor Red; exit 1 }
# Comando nativo cuja saida de erro e esperada (sondagens): pelo cmd, sem o stderr. No PowerShell 5.1 com Stop, qualquer linha no stderr
# de um programa vira erro fatal. Devolve o codigo de saida.
function Probe([string]$cmdLine) { & $env:ComSpec /c "$cmdLine >nul 2>nul"; return $LASTEXITCODE }

# --- ferramentas
Step 'ferramentas'
foreach ($t in 'git', 'python') { if (-not (Get-Command $t -ErrorAction SilentlyContinue)) { Fail "falta $t no PATH" } }
$cmake = @((Get-Command cmake -ErrorAction SilentlyContinue).Source, 'C:\Program Files\CMake\bin\cmake.exe') | Where-Object { $_ -and (Test-Path $_) } |
    Where-Object { [version](((& $_ --version)[0] -split ' ')[2] -replace '-.*$', '') -ge [version]'3.24' } | Select-Object -First 1
if (-not $cmake) { Fail 'falta CMake 3.24 ou mais novo (winget install Kitware.CMake)' }
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { Fail 'falta o Visual Studio 2022 / Build Tools com C++ (winget install Microsoft.VisualStudio.2022.BuildTools)' }
$vcvars = $null
foreach ($vs in (& $vswhere -all -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)) {
    $msvc = Get-ChildItem "$vs\VC\Tools\MSVC" -ErrorAction SilentlyContinue | Sort-Object { [version]$_.Name } | Select-Object -Last 1
    if ($msvc -and [version]$msvc.Name -ge [version]'14.40' -and (Test-Path "$vs\VC\Auxiliary\Build\vcvars64.bat")) { $vcvars = "$vs\VC\Auxiliary\Build\vcvars64.bat"; break }
}
if (-not $vcvars) { Fail 'falta um compilador C++ MSVC 14.40 ou mais novo (Visual Studio 2022 17.10+ ou Build Tools)' }
Write-Host "cmake: $cmake`nvcvars: $vcvars"

# --- disco
Step 'disco'
if (-not $Iso) { $Iso = (Get-ChildItem (Join-Path $repo 'ISO') -Filter *.iso -ErrorAction SilentlyContinue | Select-Object -First 1).FullName }
if (-not $Iso -or -not (Test-Path $Iso)) { Fail 'nenhuma .iso: ponha a sua em ISO\ ou passe -Iso' }
$disc = Join-Path $repo 'disc'
if (-not (Test-Path (Join-Path $disc 'SCUS_971.97'))) {
    $img = Mount-DiskImage -ImagePath $Iso -Access ReadOnly -PassThru
    try {
        $letter = ($img | Get-Volume).DriveLetter
        New-Item -ItemType Directory -Force $disc | Out-Null
        robocopy "${letter}:\" $disc /E /NFL /NDL /NJH /NJS /NP | Out-Null
        if ($LASTEXITCODE -ge 8) { Fail "robocopy falhou ($LASTEXITCODE)" }
    } finally { Dismount-DiskImage -ImagePath $Iso | Out-Null }
    Get-ChildItem $disc -Recurse -File | ForEach-Object { $_.IsReadOnly = $false }
}
$sha = (Get-FileHash (Join-Path $disc 'SCUS_971.97') -Algorithm SHA1).Hash.ToLower()
if ($sha -ne $elfSha1) { Fail "SCUS_971.97 com SHA1 ${sha}: o port so suporta o disco NTSC-U (SCUS-97197, $elfSha1)" }

# --- PS2Recomp com os patches do repo
Step 'PS2Recomp'
New-Item -ItemType Directory -Force $rt | Out-Null
$pr = Join-Path $rt 'PS2Recomp'
if (-not (Test-Path (Join-Path $pr '.git'))) {
    git clone -q https://github.com/ran-j/PS2Recomp.git $pr
    git -c advice.detachedHead=false -C $pr checkout -q $ps2recompCommit
}
foreach ($p in 'wotm-runtime.patch', 'ps2recomp-fpu-sqrt.patch') {
    $patch = Join-Path $repo "tools\runtime_patch\$p"
    if ((Probe "git -C `"$pr`" apply --whitespace=nowarn --reverse --check `"$patch`"") -eq 0) { Write-Host "$p ja aplicado"; continue }
    if ((Probe "git -C `"$pr`" apply --whitespace=nowarn `"$patch`"") -ne 0) { Fail "$p nao aplica em $pr (o clone mudou?)" }
    Write-Host "$p aplicado"
}

# --- scripts da pasta do runtime (dos modelos em tools\runtime_template)
Step 'scripts do runtime'
$repoFwd = $repo -replace '\\', '/'
$cmakeBin = Split-Path $cmake -Parent
foreach ($f in 'configure_dev.bat', 'build_dev.bat', 'build_tools.bat', 'run_live.ps1') {
    $text = Get-Content (Join-Path $PSScriptRoot "runtime_template\$f") -Raw
    $text = $text.Replace('@VCVARS@', $vcvars).Replace('@CMAKE_BIN@', $cmakeBin).Replace('@REPO@', $repoFwd).Replace('@ISO@', (Resolve-Path $Iso).Path.Replace("'", "''"))
    Set-Content (Join-Path $rt $f) $text -NoNewline -Encoding ascii
}
Set-Content (Join-Path $repo '.wotm_recomp_dir') $rt -Encoding ascii
New-Item -ItemType Directory -Force (Join-Path $rt 'discroot') | Out-Null
Copy-Item (Join-Path $disc 'SCUS_971.97') (Join-Path $rt 'discroot\SCUS_971.97') -Force   # o resto do disco vem da ISO (PS2X_CD_IMAGE)
& (Join-Path $PSScriptRoot 'sync_mods.ps1') -Runtime $rt -Repo $repo   # mods do repo (mods\<mod>\) para discroot\mods\

# --- jogo traduzido (codigo do jogo: so local, recomp\ no .gitignore)
Step 'jogo traduzido'
$retail = Join-Path $repo 'recomp\retail'
if (-not (Test-Path (Join-Path $retail 'register_functions.cpp'))) {
    & $env:ComSpec /c "`"$rt\build_tools.bat`" 2>&1" | Select-Object -Last 2
    if ($LASTEXITCODE -ne 0) { Fail 'build do analisador/recompilador falhou' }
    $work = Join-Path $repo 'recomp\work'
    New-Item -ItemType Directory -Force $work | Out-Null
    Copy-Item (Join-Path $disc 'SCUS_971.97') (Join-Path $work 'game.elf') -Force
    Push-Location $work
    try {
        & "$rt\build_tools\ps2xAnalyzer\ps2_analyzer.exe" game.elf config.toml | Out-Null
        if ($LASTEXITCODE -ne 0) { Fail 'ps2_analyzer falhou' }
        (Get-Content config.toml) -replace '^output = .*', 'output = "../retail/"' | Set-Content config.toml -Encoding ascii
        & "$rt\build_tools\ps2xRecomp\ps2_recomp.exe" config.toml | Out-Null
        if ($LASTEXITCODE -ne 0) { Fail 'ps2_recomp falhou' }
    } finally { Pop-Location }
}
$cpp = (Get-ChildItem $retail -Filter *.cpp).Count
Write-Host "$cpp arquivos traduzidos"

# --- build
if (-not $SkipBuild) {
    Step 'configurar e compilar (o primeiro build leva alguns minutos)'
    if (-not (Test-Path (Join-Path $rt 'build_dev\build.ninja'))) {
        & $env:ComSpec /c "`"$rt\configure_dev.bat`" 2>&1" | Select-Object -Last 2
        if (-not (Test-Path (Join-Path $rt 'build_dev\build.ninja'))) { Fail 'configure_dev.bat falhou' }
    }
    & $env:ComSpec /c "`"$repo\build_runtime.bat`" 2>&1"
    if ($LASTEXITCODE -ne 0) { Fail "build falhou: veja $rt\build_dev.log" }
}
Step 'pronto'
Write-Host "Jogar: run_live.bat -Auto 0   (ou tools\launcher\build.bat para o launcher)"
