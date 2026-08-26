$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$toolBin = if ($env:CC65_BIN) { $env:CC65_BIN } else { "C:\tools\cc65\bin" }
$ca65 = Join-Path $toolBin "ca65.exe"
$cl65 = Join-Path $toolBin "cl65.exe"

function New-ChasedMemoryMap {
    $mapPath = Join-Path $root "Chased.map"
    $sourcePath = Join-Path $root "main.c"
    $outputPath = Join-Path $root "Chased_memory_map.html"
    $mapLines = Get-Content -Path $mapPath
    $sourceText = Get-Content -Path $sourcePath -Raw
    $builder = New-Object System.Text.StringBuilder
    $scale = 900.0 / 65536.0

    $regions = @(
        @{ Name = "Zero page and OS work area"; Start = 0x0000; End = 0x02FF; Color = "#f4b942"; Kind = "System" },
        @{ Name = "6502 hardware stack"; Start = 0x0100; End = 0x01FF; Color = "#f4b942"; Kind = "Stack" },
        @{ Name = "OS shadow and display variables"; Start = 0x0200; End = 0x04FF; Color = "#f4b942"; Kind = "OS" },
        @{ Name = "Display list and screen control"; Start = 0x0230; End = 0x0231; Color = "#f4b942"; Kind = "ANTIC" },
        @{ Name = "GTIA registers"; Start = 0xD000; End = 0xD01F; Color = "#e76f51"; Kind = "GTIA" },
        @{ Name = "ANTIC registers"; Start = 0xD400; End = 0xD40F; Color = "#2a9d8f"; Kind = "ANTIC" },
        @{ Name = "POKEY registers"; Start = 0xD200; End = 0xD20F; Color = "#457b9d"; Kind = "POKEY" },
        @{ Name = "PIA registers"; Start = 0xD300; End = 0xD30F; Color = "#6d597a"; Kind = "PIA" },
        @{ Name = "PMG runtime workspace"; Start = 0x7000; End = 0x77FF; Color = "#e9c46a"; Kind = "PMG" },
        @{ Name = "BASIC ROM"; Start = 0xA000; End = 0xBFFF; Color = "#9b5de5"; Kind = "ROM" },
        @{ Name = "OS ROM and hardware area"; Start = 0xC000; End = 0xFFFF; Color = "#9b5de5"; Kind = "ROM" }
    )

    foreach ($match in [regex]::Matches(($mapLines -join "`n"), '(?m)^(LOWCODE|ONCE|CODE|RODATA|DATA|BSS)\s+([0-9A-F]{6})\s+([0-9A-F]{6})\s+([0-9A-F]{6})')) {
        $start = [Convert]::ToInt32($match.Groups[2].Value, 16)
        $end = [Convert]::ToInt32($match.Groups[3].Value, 16)
        if ($end -gt $start) {
            $regions += @{ Name = "Program $($match.Groups[1].Value)"; Start = $start; End = $end; Color = "#264653"; Kind = "Program" }
        }
    }

    $bssStartMatch = [regex]::Match(($mapLines -join "`n"), '(?m)^__BSS_RUN__\s+([0-9A-F]{6})\s+RLA')
    $bssSizeMatch = [regex]::Match(($mapLines -join "`n"), '(?m)^__BSS_SIZE__\s+([0-9A-F]{6})\s+REA')
    $bssStart = if ($bssStartMatch.Success) { [Convert]::ToInt32($bssStartMatch.Groups[1].Value, 16) } else { 0 }
    $bssSize = if ($bssSizeMatch.Success) { [Convert]::ToInt32($bssSizeMatch.Groups[1].Value, 16) } else { 0 }
    $programEnd = $bssStart + $bssSize
    $ramCeiling = 0xA000
    $freeBytes = [math]::Max(0, $ramCeiling - $programEnd)
    $freeKiB = [math]::Round($freeBytes / 1024.0, 1)
    $programEndText = ('$' + $programEnd.ToString('X4'))
    $freeStartText = ('$' + $programEnd.ToString('X4'))
    $freeEndText = ('$' + ($ramCeiling - 1).ToString('X4'))

    $functions = @()
    foreach ($match in [regex]::Matches($sourceText, '(?m)^static\s+(?:unsigned char|unsigned int|void)\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(')) {
        $functions += $match.Groups[1].Value
    }
    $functions += "main"
    $functionRows = ($functions | Sort-Object -Unique | ForEach-Object { "<li><code>$([System.Net.WebUtility]::HtmlEncode($_))</code></li>" }) -join ""
    $regionRows = ($regions | Sort-Object @{ Expression = { [int]$_.Start } }, @{ Expression = { [int]$_.End } }, Name | ForEach-Object {
        $regionName = [System.Net.WebUtility]::HtmlEncode($_.Name)
        $regionStart = ('$' + $_.Start.ToString('X4'))
        $regionEnd = ('$' + $_.End.ToString('X4'))
        $regionSize = ([int]$_.End - [int]$_.Start + 1)
        "<tr><td>$regionName</td><td><code>$regionStart-$regionEnd</code></td><td>$regionSize</td><td>$($_.Kind)</td></tr>"
    }) -join ""

    [void]$builder.AppendLine("<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>Game Atari 800XL Memory Map</title>")
    [void]$builder.AppendLine("<style>body{margin:0;background:#101820;color:#e9ecef;font:15px Segoe UI,Arial,sans-serif}main{max-width:1180px;margin:0 auto;padding:32px}h1{font-size:30px;margin:0 0 8px;color:#f4b942}p{color:#b8c4ce}.map{background:#182631;border:1px solid #3b5361;border-radius:12px;padding:20px;box-shadow:0 16px 40px #0005}svg{display:block;width:100%;height:auto}.axis{fill:#9fb3bf;font-size:12px}.label{fill:#fff;font-size:12px;font-weight:600}.legend{display:flex;flex-wrap:wrap;gap:12px;margin-top:18px}.legend span{display:inline-flex;align-items:center;gap:6px;color:#c9d4da}.swatch{width:13px;height:13px;border-radius:3px;display:inline-block}section{margin-top:26px;background:#182631;border:1px solid #3b5361;border-radius:12px;padding:20px}h2{color:#f4b942;margin-top:0}code{color:#9be7d8}ul{columns:3;line-height:1.8;margin:0;padding-left:22px}table{width:100%;border-collapse:collapse}th,td{text-align:left;padding:8px 10px;border-bottom:1px solid #304955}th{color:#f4b942}td:last-child{color:#9fb3bf}@media(max-width:700px){main{padding:18px}ul{columns:1}table{font-size:13px}}</style></head><body><main><h1>Chased - Atari 800XL memory map</h1><p>Generated after a successful build from <code>Chased.map</code> and <code>main.c</code>. The full 64 KiB address space is shown from <code>`$0000</code> to <code>`$FFFF</code>.</p><section><h2>Program growth budget</h2><p>Linked program end: <code>$programEndText</code>. Estimated free RAM before the BASIC ROM boundary at <code>`$A000</code>: <strong>$freeBytes bytes ($freeKiB KiB)</strong>, address range <code>$freeStartText-$freeEndText</code>.</p></section><div class='map'><svg viewBox='0 0 1100 980' role='img' aria-label='Atari 800XL 64 kilobyte memory map'><rect x='180' y='40' width='700' height='900' rx='8' fill='#0d141a' stroke='#58727f'/>")
    foreach ($tick in 0..8) {
        $address = $tick * 0x2000
        $y = 40 + [math]::Round($address * $scale)
        $label = ('$' + $address.ToString('X4'))
        [void]$builder.AppendLine("<line x1='180' y1='$y' x2='880' y2='$y' stroke='#29414d'/><text class='axis' x='120' y='$($y + 4)'>$label</text>")
    }
    foreach ($region in $regions) {
        $y = 40 + [math]::Round($region.Start * $scale)
        $height = [math]::Max(3, [math]::Round(($region.End - $region.Start + 1) * $scale))
        $label = [System.Net.WebUtility]::HtmlEncode($region.Name)
        $address = ('$' + $region.Start.ToString('X4') + '-$' + $region.End.ToString('X4'))
        $regionLabel = if ($height -ge 18) { "<text class='label' x='205' y='$([math]::Min(930, $y + [math]::Max(13, $height / 2)))'>$label</text>" } else { "" }
        [void]$builder.AppendLine("<rect x='190' y='$y' width='680' height='$height' rx='3' fill='$($region.Color)' opacity='.9'><title>$label $address</title></rect>$regionLabel")
    }
    [void]$builder.AppendLine("<text class='axis' x='900' y='55'>High</text><text class='axis' x='900' y='938'>Low</text></svg><div class='legend'><span><i class='swatch' style='background:#264653'></i>Program/linker sections</span><span><i class='swatch' style='background:#e76f51'></i>GTIA</span><span><i class='swatch' style='background:#2a9d8f'></i>ANTIC</span><span><i class='swatch' style='background:#457b9d'></i>POKEY</span><span><i class='swatch' style='background:#6d597a'></i>PIA</span><span><i class='swatch' style='background:#9b5de5'></i>ROM</span></div></div><section><h2>Mapped regions</h2><table><thead><tr><th>Region</th><th>Address range</th><th>Size (bytes)</th><th>Class</th></tr></thead><tbody>$regionRows</tbody></table></section><section><h2>Linker sections</h2><table><thead><tr><th>Section</th><th>Meaning</th></tr></thead><tbody><tr><td><code>LOWCODE</code></td><td>Small, frequently called routines placed in the low-code area for compact or fast access.</td></tr><tr><td><code>ONCE</code></td><td>Initialization code that can be discarded or reused after startup.</td></tr><tr><td><code>CODE</code></td><td>Normal executable code: the main game loop, movement, collision, rendering, and other routines that remain resident.</td></tr><tr><td><code>RODATA</code></td><td>Read-only constants, sprite bytes, lookup tables, and compiled level data.</td></tr><tr><td><code>DATA</code></td><td>Initialized writable global and static variables copied into RAM at startup.</td></tr><tr><td><code>BSS</code></td><td>Zero-initialized writable variables, including game state and runtime buffers; it occupies RAM but not XEX file space.</td></tr></tbody></table></section><section><h2>Program functions</h2><ul>$functionRows</ul></section><section><h2>Important hardware addresses</h2><p><code>ANTIC</code> display list, vertical scroll, DMA control, and PMBASE; <code>GTIA</code> player positions, colors, PRIOR, and GRACTL; <code>POKEY</code> sound registers; <code>PIA</code> and OS shadow registers are all represented in the address-space view.</p></section></main></body></html>")
    Set-Content -Path $outputPath -Value $builder.ToString() -Encoding UTF8
}

if (-not (Test-Path $ca65) -or -not (Test-Path $cl65)) {
    throw "cc65 tools not found in $toolBin. Set CC65_BIN or install ca65.exe and cl65.exe there."
}

Push-Location $root
try {
    & $ca65 -g INTERRUPTS.ASM -o INTERRUPTS.o
    if ($LASTEXITCODE -ne 0) {
        throw "ca65 failed with exit code $LASTEXITCODE."
    }
    & $ca65 -g Assembly.ASM -o Assembly.o
    if ($LASTEXITCODE -ne 0) {
        throw "ca65 failed with exit code $LASTEXITCODE."
    }
    & $cl65 -t atari -C ataridos25-xex.cfg --start-addr 0x4000 -O -g `
        -m Chased.map -Ln Chased.lbl -o Chased.XEX main.c melody.c collision.c pmg.c display.c charset.c INTERRUPTS.o Assembly.o
    if ($LASTEXITCODE -ne 0) {
        throw "cl65 failed with exit code $LASTEXITCODE."
    }
    New-ChasedMemoryMap
}
finally {
    Pop-Location
}
