$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$toolBin = if ($env:CC65_BIN) { $env:CC65_BIN } else { "C:\tools\cc65\bin" }
$ca65 = Join-Path $toolBin "ca65.exe"
$cl65 = Join-Path $toolBin "cl65.exe"
$buildNumberPath = Join-Path $root "build_number.txt"
$buildHeaderPath = Join-Path $root "build_number.h"

if (-not (Test-Path $ca65) -or -not (Test-Path $cl65)) {
    throw "cc65 tools not found in $toolBin. Set CC65_BIN or install ca65.exe and cl65.exe there."
}

$currentBuild = 0
if (Test-Path $buildNumberPath) {
    if (-not [int]::TryParse((Get-Content -Raw $buildNumberPath).Trim(), [ref]$currentBuild) -or $currentBuild -lt 0) {
        throw "build_number.txt must contain a non-negative integer."
    }
}
$nextBuild = $currentBuild + 1
$displayBuild = $nextBuild % 1000
$buildHeader = @"
#ifndef BUILD_NUMBER_H
#define BUILD_NUMBER_H

#define BUILD_DIGIT_100 $([int][Math]::Floor($displayBuild / 100))
#define BUILD_DIGIT_10 $([int]([Math]::Floor($displayBuild / 10) % 10))
#define BUILD_DIGIT_1 $($displayBuild % 10)

#endif
"@
[System.IO.File]::WriteAllText($buildHeaderPath, $buildHeader)

Push-Location $root
try {
    & $ca65 -g BOOT.ASM -o BOOT.o
    if ($LASTEXITCODE -ne 0) {
        throw "ca65 failed with exit code $LASTEXITCODE."
    }
    & $ca65 -g COLUMN3D.ASM -o COLUMN3D.o
    if ($LASTEXITCODE -ne 0) {
        throw "ca65 failed with exit code $LASTEXITCODE."
    }
    & $ca65 -g RAYCAST.ASM -o RAYCAST.o
    if ($LASTEXITCODE -ne 0) {
        throw "ca65 failed with exit code $LASTEXITCODE."
    }
    & $ca65 -g MELODY.ASM -o MELODY.o
    if ($LASTEXITCODE -ne 0) {
        throw "ca65 failed with exit code $LASTEXITCODE."
    }
    & $ca65 -g FLOORDLI.ASM -o FLOORDLI.o
    if ($LASTEXITCODE -ne 0) {
        throw "ca65 failed with exit code $LASTEXITCODE."
    }
    & $ca65 -g SPLASHRAINBOW.ASM -o SPLASHRAINBOW.o
    if ($LASTEXITCODE -ne 0) {
        throw "ca65 failed with exit code $LASTEXITCODE."
    }
    & .\convert_splash_mono.ps1
    if ($LASTEXITCODE -ne 0) {
        throw "Splash artwork conversion failed with exit code $LASTEXITCODE."
    }
    & $cl65 -t atari -C chased3d.cfg --start-addr 0x4000 -O -g `
        -m Chased3D.map -Ln Chased3D.lbl -o Chased3D.XEX chased3d.c view3d.c textplot.c maze.c trig3d.c sprite3d.c melody.c splash_screen.c BOOT.o COLUMN3D.o RAYCAST.o MELODY.o FLOORDLI.o SPLASHRAINBOW.o
    if ($LASTEXITCODE -ne 0) {
        throw "cl65 failed with exit code $LASTEXITCODE."
    }
    & .\report_free_ram.ps1 -MapPath .\Chased3D.map

    & .\update_dos25_atr.ps1 -AtrPath .\Chased3D.atr -RenameFiles @{
        "CHASED3D.XEX" = "AUTORUN.SYS"
    } -Files @{
        "AUTORUN.SYS" = ".\Chased3D.XEX"
        "L2.CSV" = ".\L2.csv"
        "L3.CSV" = ".\L3.csv"
        "L4.CSV" = ".\L4.csv"
        "L5.CSV" = ".\L5.csv"
        "SPLASH.BMP" = ".\SPLASH.BMP"
    }
    [System.IO.File]::WriteAllText($buildNumberPath, "$nextBuild`r`n")
    Write-Host "Successful build number: $nextBuild"
}
finally {
    Pop-Location
}
