# ABOUTME: Setup for the wotlane friends package (tools\package_friends.ps1): finds the friend's own ISO, copies its SCUS_971.97 to
# ABOUTME: runtime\discroot, points run_live.ps1 at the ISO and writes .wotm_recomp_dir. Run by Setup.bat; safe to run again.
param([string]$Iso = '', [switch]$NoPause)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$elfSha1 = '1ed7f8457436a9170d8f85386e267369c7fcdd4f'   # SCUS_971.97 from the NTSC-U disc (SCUS-97197)

function Done([int]$code, [string]$msg) {
    Write-Host $msg -ForegroundColor $(if ($code) { 'Red' } else { 'Green' })
    if (-not $NoPause) { Read-Host 'Press Enter to close' | Out-Null }
    exit $code
}

try {
    # downloaded zip: Windows marks every file as coming from the internet and blocks scripts and DLLs
    Get-ChildItem $root -Recurse -File | Unblock-File

    if (-not $Iso) { $Iso = (Get-ChildItem (Join-Path $root 'ISO') -Filter *.iso -ErrorAction SilentlyContinue | Select-Object -First 1).FullName }
    if (-not $Iso) {
        Add-Type -AssemblyName System.Windows.Forms
        $dlg = New-Object System.Windows.Forms.OpenFileDialog -Property @{ Title = 'Your War of the Monsters (USA) ISO'; Filter = 'Disc image (*.iso)|*.iso' }
        if ($dlg.ShowDialog() -eq 'OK') { $Iso = $dlg.FileName }
    }
    if (-not $Iso -or -not (Test-Path -LiteralPath $Iso)) { Done 1 'No ISO: put your War of the Monsters (USA) .iso in the ISO folder and run Setup.bat again.' }
    $Iso = (Resolve-Path -LiteralPath $Iso).Path
    Write-Host "ISO: $Iso"

    $disc = Join-Path $root 'runtime\discroot'
    New-Item -ItemType Directory -Force $disc | Out-Null
    $elf = Join-Path $disc 'SCUS_971.97'
    $img = Mount-DiskImage -ImagePath $Iso -Access ReadOnly -PassThru
    try {
        $letter = ($img | Get-Volume).DriveLetter
        Copy-Item "${letter}:\SCUS_971.97" $elf -Force
    } finally { Dismount-DiskImage -ImagePath $Iso | Out-Null }
    (Get-Item $elf).IsReadOnly = $false
    $sha = (Get-FileHash $elf -Algorithm SHA1).Hash.ToLower()
    if ($sha -ne $elfSha1) { Remove-Item $elf -Force; Done 1 'This ISO is not War of the Monsters NTSC-U (SCUS-97197): only the USA disc works.' }

    $template = Get-Content (Join-Path $root 'runtime\run_live.template.ps1') -Raw
    Set-Content (Join-Path $root 'runtime\run_live.ps1') $template.Replace('@ISO@', $Iso.Replace("'", "''")) -Encoding utf8 -NoNewline
    Set-Content (Join-Path $root '.wotm_recomp_dir') (Join-Path $root 'runtime') -Encoding ascii
    Done 0 'Setup done. Open WotM_Launcher.exe (see README.txt for playing online).'
} catch {
    Done 1 "Setup failed: $($_.Exception.Message)"
}
