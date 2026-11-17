# ABOUTME: Determinism test for the netcode: runs the same fight twice (the second with slow frames, WOTM_LOGICDELAY) in deterministic
# ABOUTME: mode (WOTM_FIXEDSTEP + WOTM_SEED) and compares the per-frame state hash (tools\statediff.py). Exits 0 if identical.
#
# Usage: powershell -File tools\determinism_check.ps1 [-Match 3,1,1] [-Players 2] [-Secs 60] [-Delay 40] [-Step 1]
#   -Match/-Players: the fight (WOTM_MATCH/WOTM_PLAYERS; AI on everyone's pad). -Delay: maximum delay in ms for the second run.
#   The hashes go to <runtime>\det_A.hash and det_B.hash; run tools\statediff.py with --dumps to see what diverges.
#   -Replay: the players stay on the pad (no AI): run A plays a PS2X_PAD script (time-based) and records every frame's input
#     (WOTM_INPUTREC); run B, with slow frames, replays the recording (WOTM_INPUTPLAY). Tests the netcode's per-frame input.
#   -InputDelay <n> (with -Replay): run A applies local input n frames after sampling it (WOTM_INPUTDELAY, the lockstep
#     delay); checks in the log that the first input sampled on frame X takes effect and is applied on frame X + n.
param(
    [string]$Match = '3,1,1',
    [int]$Players = 2,
    [int]$Secs = 60,
    [int]$Delay = 40,
    [int]$Step = 1,
    [switch]$Replay,
    [int]$InputDelay = 0
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$runtime = if ($env:WOTM_RECOMP_DIR) { $env:WOTM_RECOMP_DIR } elseif (Test-Path "$repo\.wotm_recomp_dir") { (Get-Content "$repo\.wotm_recomp_dir" -TotalCount 1).Trim() } else { '' }
if (-not $runtime -or -not (Test-Path "$runtime\run_live.ps1")) { throw "Runtime folder not found: run run_live.bat once or set WOTM_RECOMP_DIR." }

$base = "WOTM_MATCH=$Match;WOTM_PLAYERS=$Players;WOTM_NOINPUT=1;WOTM_FIXEDSTEP=$Step;WOTM_SEED=1234"
if ($Replay) {
    $inputs = Join-Path $runtime 'det_inputs.bin'
    # walk, punch, jump and grab, for players 1 and 2 (seconds since the window opened; the fight starts at about 15 s)
    $script = '18:6:up;20:1:square;22:1:cross;24:3:left;25:1:triangle;27:4:up@2;28:1:square@2;30:2:right@2;31:1:circle@2;33:5:down;34:1:square;36:1:triangle@2;38:4:left@2'
    $runs = @(   # the script contains ';' and does not fit in -Env: it goes through the environment variable (Pad)
        @{ Name = 'A'; Pad = $script; Extra = "WOTM_LOGICDELAY=;WOTM_AIVSAI=;WOTM_INPUTREC=$inputs;WOTM_INPUTPLAY=;WOTM_INPUTDELAY=$InputDelay" },
        @{ Name = 'B'; Pad = ''; Extra = "WOTM_LOGICDELAY=$Delay;WOTM_AIVSAI=;WOTM_INPUTREC=;WOTM_INPUTPLAY=$inputs;WOTM_INPUTDELAY=" }
    )
} else {
    $runs = @(
        @{ Name = 'A'; Extra = 'WOTM_LOGICDELAY=;WOTM_AIVSAI=1' },
        @{ Name = 'B'; Extra = "WOTM_LOGICDELAY=$Delay;WOTM_AIVSAI=1" }
    )
}
foreach ($r in $runs) {
    $hash = Join-Path $runtime "det_$($r.Name).hash"
    if (Test-Path $hash) { Remove-Item $hash -Force }
    $env:PS2X_PAD = if ($r.Pad) { $r.Pad } else { $null }
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'live_check.ps1') -Env "$base;$($r.Extra);WOTM_STATEHASH=$hash" -Secs $Secs -Expect '\[sync\] seed 1234' | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Host "run $($r.Name) failed (see $runtime\live_check.log)" -ForegroundColor Red; exit 1 }
    Copy-Item (Join-Path $runtime 'live_check.log') (Join-Path $runtime "det_$($r.Name).log") -Force
}
if ($Replay) {
    # A samples on frame X and it takes effect at X + delay; A and B apply the first non-neutral input on the same frame
    $logA = Get-Content (Join-Path $runtime 'det_A.log') -Raw
    $logB = Get-Content (Join-Path $runtime 'det_B.log') -Raw
    if ($logA -notmatch "\[sync\] input delay: $InputDelay frame") { Write-Host "FAILED: A did not announce delay $InputDelay" -ForegroundColor Red; exit 1 }
    if ($logA -notmatch '\[sync\] first local input: sample of frame (\d+) applies at frame (\d+)') { Write-Host 'FAILED: A did not sample local input' -ForegroundColor Red; exit 1 }
    $sampled = [int]$Matches[1]; $scheduled = [int]$Matches[2]
    if ($scheduled -ne $sampled + $InputDelay) { Write-Host "FAILED: sample from frame $sampled scheduled for $scheduled (delay $InputDelay)" -ForegroundColor Red; exit 1 }
    $applied = @()
    foreach ($l in @($logA, $logB)) {
        if ($l -notmatch '\[sync\] first non-neutral input applied at frame (\d+)') { Write-Host 'FAILED: input never applied' -ForegroundColor Red; exit 1 }
        $applied += [int]$Matches[1]
    }
    if ($applied[0] -ne $scheduled -or $applied[1] -ne $scheduled) { Write-Host "FAILED: scheduled for $scheduled, applied at A $($applied[0]) and B $($applied[1])" -ForegroundColor Red; exit 1 }
    Write-Host "ok      input sampled on frame $sampled applied on $scheduled (delay $InputDelay) in A and B"
}
& python (Join-Path $PSScriptRoot 'statediff.py') (Join-Path $runtime 'det_A.hash') (Join-Path $runtime 'det_B.hash')
if ($LASTEXITCODE -ne 0) { Write-Host 'FAILED: the state diverges' -ForegroundColor Red; exit 1 }
Write-Host 'PASSED: identical state on every compared frame'
exit 0
