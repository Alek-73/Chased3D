$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$toolBin = if ($env:CC65_BIN) { $env:CC65_BIN } else { "C:\tools\cc65\bin" }
$cc65 = Join-Path $toolBin "cc65.exe"
$ca65 = Join-Path $toolBin "ca65.exe"
$cl65 = Join-Path $toolBin "cl65.exe"
$debugDir = Join-Path $root "debug"
$debugAtr = Join-Path $debugDir "Chased3D-Debug.atr"

foreach ($tool in @($cc65, $ca65, $cl65)) {
    if (-not (Test-Path $tool)) {
        throw "cc65 tool not found: $tool. Set CC65_BIN or install cc65 in $toolBin."
    }
}
if (-not (Test-Path (Join-Path $root "build_number.h"))) {
    throw "build_number.h is missing. Run build_chased3d.ps1 once before the debug build."
}

New-Item -ItemType Directory -Force $debugDir | Out-Null

& (Join-Path $root "convert_splash_mono.ps1")
if ($LASTEXITCODE -ne 0) {
    throw "Splash artwork conversion failed with exit code $LASTEXITCODE."
}

function Invoke-Tool([string]$tool, [string[]]$arguments) {
    & $tool @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$(Split-Path -Leaf $tool) failed with exit code $LASTEXITCODE."
    }
}

$cSources = @("chased3d.c", "view3d.c", "textplot.c", "maze.c", "trig3d.c", "sprite3d.c", "melody.c", "splash_screen.c")
$asmSources = @("BOOT.ASM", "COLUMN3D.ASM", "RAYCAST.ASM", "MELODY.ASM", "FLOORDLI.ASM", "SPLASHRAINBOW.ASM")
$objects = [System.Collections.Generic.List[string]]::new()

foreach ($sourceName in $cSources) {
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($sourceName)
    $assemblyPath = Join-Path $debugDir "$baseName.s"
    $objectPath = Join-Path $debugDir "$baseName.o"
    $listingPath = Join-Path $debugDir "$baseName.lst"

    $compilerArguments = @(
        "-t", "atari",
        "-O",
        "-g",
        "-T",
        "-D", "DEBUG_HUD",
        "-I", $root,
        "-o", $assemblyPath,
        (Join-Path $root $sourceName)
    )
    Invoke-Tool $cc65 $compilerArguments
    Invoke-Tool $ca65 @(
        "-g",
        "-l", $listingPath,
        "-o", $objectPath,
        $assemblyPath
    )
    $objects.Add($objectPath)
}

foreach ($sourceName in $asmSources) {
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($sourceName)
    $objectPath = Join-Path $debugDir "${baseName}_asm.o"
    $listingPath = Join-Path $debugDir "${baseName}_asm.lst"

    Invoke-Tool $ca65 @(
        "-g",
        "-l", $listingPath,
        "-o", $objectPath,
        (Join-Path $root $sourceName)
    )
    $objects.Add($objectPath)
}

$xexPath = Join-Path $debugDir "Chased3D-Debug.XEX"
$mapPath = Join-Path $debugDir "Chased3D-Debug.map"
$labelPath = Join-Path $debugDir "Chased3D-Debug.lbl"
$debugInfoPath = Join-Path $debugDir "Chased3D-Debug.dbg"
$linkArguments = @(
    "-t", "atari",
    "-C", (Join-Path $root "chased3d.cfg"),
    "--start-addr", "0x4000",
    "-g",
    "-m", $mapPath,
    "-Ln", $labelPath,
    "-Wl", "--dbgfile,$debugInfoPath",
    "-o", $xexPath
) + $objects.ToArray()
Invoke-Tool $cl65 $linkArguments
& (Join-Path $root "report_free_ram.ps1") -MapPath $mapPath

Copy-Item -Force (Join-Path $root "Chased3D.atr") $debugAtr
& (Join-Path $root "update_dos25_atr.ps1") -AtrPath $debugAtr -RenameFiles @{
    "CHASED3D.XEX" = "AUTORUN.SYS"
} -Files @{
    "AUTORUN.SYS" = $xexPath
    "L2.CSV" = (Join-Path $root "L2.csv")
    "L3.CSV" = (Join-Path $root "L3.csv")
    "L4.CSV" = (Join-Path $root "L4.csv")
    "L5.CSV" = (Join-Path $root "L5.csv")
    "SPLASH.BMP" = (Join-Path $root "SPLASH.BMP")
}
if ($LASTEXITCODE -ne 0) {
    throw "DOS 2.5 ATR update failed with exit code $LASTEXITCODE."
}

Write-Host "Altirra source-debug build created:"
Write-Host "  ATR: $debugAtr"
Write-Host "  XEX: $xexPath"
Write-Host "  DBG: $debugInfoPath"
Write-Host "Load the .dbg file in Altirra after booting the debug ATR."