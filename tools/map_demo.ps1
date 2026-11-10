# ABOUTME: Writes the map creator's demo map (CENTRAL with towers in the park, a wall of towers and taller blocks) into a stages folder.
# ABOUTME: It runs stage_map.py make with fixed removes and copies; no map folder is committed under mods/ (plan 2026-11-10, decision 18).
#
# Usage: powershell -File tools\map_demo.ps1 -Out <mods\<mod>\stages> [-Disc disc] [-Id demo] [-Name 'TOWER PARK']
# Then: WOTM_MODS=<the mods folder>, and the map shows in the stage select (or WOTM_MATCH=3,1,<id>).
#
# Layout (CENTRAL; places from tools\stage_map.py pieces and the stage's collision boxes; footprints are the boxes taller
# than 60, without the sidewalk slabs):
# - Park (x -537..461, y -324..221): its 17 props are removed and three of the tallest copyable towers stand in a row in
#   it, 200 apart (a street's width), with the park's south strip open as the lane along the wall.
# - Wall: the street south of the park (y -531..-329) holds five slim towers edge to edge, in a run of 2 and a run of 3;
#   the street west of the park stays open as the lane through it.
# - Three low blocks (0x2d85a0 north of the park, 0x1fe2d0 east of it, 0x275340 in the south-east) are removed and a tower
#   copy stands in each one's place: the street plan stays as it was.
# Every start point is at least 300 from every copy.
# Riders are on: the towers' rooftop props and power-up places are copied with them.
param(
    [Parameter(Mandatory = $true)][string]$Out,
    [string]$Disc = 'disc',
    [string]$Id = 'demo',
    [string]$Name = 'TOWER PARK'
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent

$removes = @(
    # the park's props (benches, kiosks, the truck...): all 17 destructible pieces on its lawn
    '0x627e40', '0x62d470', '0x634460', '0x636f60', '0x63fb60', '0x644070', '0x649190', '0x64e750', '0x6527e0',
    '0x657890', '0x65d040', '0x65fa60', '0x662520', '0x668460', '0x66b380', '0x66c800', '0x670e40',
    # low blocks a tower takes the place of (roofs 148, 171 and 188)
    '0x2d85a0', '0x1fe2d0', '0x275340'
)
# 0x..=x,y,z,turn: x y z is the piece's own place (its node), so the footprint centres below come out as noted
$copies = @(
    # park, west to east, 200 apart (the streets' width): 0x37d6e0 (roof 418) footprint x -537..-316 y -100..148,
    # 0x3db080 (roof 402) x -116..70 y -40..191, 0x327de0 (roof 319) x 270..460 y -110..180
    '0x37d6e0=-426.5,24.5,155.8,0',
    '0x3db080=-18.1,80.9,147.4,0',
    '0x327de0=357.9,32.2,159.3,180',
    # wall in the street south of the park (y -531..-329), west to east: 0x516540 (roof 287) x -1195..-978, 0x59f340
    # (roof 219) -978..-758; lane (the street west of the park, x -758..-565); 0x59f340 -565..-345, 0x516540 -345..-128,
    # 0x555240 (roof 294) -128..248 (start point 5 at 300)
    '0x516540=-977.2,-502.5,0,180',
    '0x59f340=-768.9,-514.6,0,270',
    '0x59f340=-554.1,-345.4,0,90',
    '0x516540=-345.8,-357.5,0,0',
    '0x555240=-120.5,-511.6,0,90',
    # taller blocks: 0x351360 (roof 405) where 0x2d85a0 stood (x -253..57 y 404..647), 0x3db080 where 0x1fe2d0 stood
    # (x 657..843 y -104..128), 0x327de0 where 0x275340 stood (x 683..873 y -998..-708)
    '0x351360=-94.7,523.4,202.4,90',
    '0x3db080=745.1,6.6,147.4,180',
    '0x327de0=785.1,-850.2,159.3,0'
)

$argv = @("$repo\tools\stage_map.py", 'make', '--base', 'CENTRAL', '--id', $Id, '--name', $Name, '--out', $Out, '--disc', $Disc)
foreach ($r in $removes) { $argv += @('--remove', $r) }
foreach ($c in $copies) { $argv += @('--copy', $c) }
& python @argv
if ($LASTEXITCODE -ne 0) { throw "stage_map.py make failed ($LASTEXITCODE)" }
