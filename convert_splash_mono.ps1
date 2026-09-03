param(
    [string]$SourcePath = (Join-Path $PSScriptRoot "chased_splash.bmp"),
    [string]$OutputPath = (Join-Path $PSScriptRoot "SPLASH.BMP")
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$width = 160
$height = 46
$rowBytes = $width / 2
$pixelOffset = 70
$fileSize = $pixelOffset + ($rowBytes * $height)
$source = [Drawing.Bitmap]::FromFile((Resolve-Path $SourcePath))
$scaled = New-Object Drawing.Bitmap $width, $height, ([Drawing.Imaging.PixelFormat]::Format24bppRgb)

try {
    $graphics = [Drawing.Graphics]::FromImage($scaled)
    try {
        $graphics.Clear([Drawing.Color]::Black)
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.DrawImage($source, 0, 0, $width, $height)
    }
    finally {
        $graphics.Dispose()
    }

    $indices = New-Object 'byte[,]' $height, $width
    for ($y = 0; $y -lt $height; ++$y) {
        for ($x = 0; $x -lt $width; ++$x) {
            $color = $scaled.GetPixel($x, $y)
            $luminance = (30 * $color.R + 59 * $color.G + 11 * $color.B) / 100
            $indices[$y, $x] = if ($luminance -ge 128) { 1 } else { 0 }
        }
    }

    $stream = [IO.File]::Open($OutputPath, [IO.FileMode]::Create)
    $writer = New-Object IO.BinaryWriter $stream
    try {
        $writer.Write([byte][char]'B')
        $writer.Write([byte][char]'M')
        $writer.Write([int]$fileSize)
        $writer.Write([int]0)
        $writer.Write([int]$pixelOffset)
        $writer.Write([int]40)
        $writer.Write([int]$width)
        $writer.Write([int]$height)
        $writer.Write([Int16]1)
        $writer.Write([Int16]4)
        $writer.Write([int]0)
        $writer.Write([int]($rowBytes * $height))
        $writer.Write([int]0)
        $writer.Write([int]0)
        $writer.Write([int]4)
        $writer.Write([int]4)

        foreach ($entry in @(
            @(8, 8, 12, 0),
            @(32, 40, 204, 0),
            @(212, 70, 184, 0),
            @(40, 220, 240, 0))) {
            foreach ($component in $entry) { $writer.Write([byte]$component) }
        }

        for ($y = $height - 1; $y -ge 0; --$y) {
            for ($x = 0; $x -lt $width; $x += 2) {
                $nextX = $x + 1
                $writer.Write([byte](($indices[$y, $x] -shl 4) -bor
                                     $indices[$y, $nextX]))
            }
        }
    }
    finally {
        $writer.Dispose()
    }
}
finally {
    $scaled.Dispose()
    $source.Dispose()
}

Write-Host "Converted monochrome splash artwork: $OutputPath"
