$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$toolBin = if ($env:CC65_BIN) { $env:CC65_BIN } else { "C:\tools\cc65\bin" }
$ca65 = Join-Path $toolBin "ca65.exe"
$cl65 = Join-Path $toolBin "cl65.exe"

if (-not (Test-Path $ca65) -or -not (Test-Path $cl65)) {
    throw "cc65 tools not found in $toolBin. Set CC65_BIN or install ca65.exe and cl65.exe there."
}

Push-Location $root
try {
    & $ca65 -g COLUMN3D.ASM -o COLUMN3D.o
    if ($LASTEXITCODE -ne 0) {
        throw "ca65 failed with exit code $LASTEXITCODE."
    }
    & $ca65 -g RAYCAST.ASM -o RAYCAST.o
    if ($LASTEXITCODE -ne 0) {
        throw "ca65 failed with exit code $LASTEXITCODE."
    }
    & $cl65 -t atari -C chased3d.cfg --start-addr 0x4000 -O -g `
        -m Chased3D.map -Ln Chased3D.lbl -o Chased3D.XEX chased3d.c view3d.c maze.c trig3d.c COLUMN3D.o RAYCAST.o
    if ($LASTEXITCODE -ne 0) {
        throw "cl65 failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}
