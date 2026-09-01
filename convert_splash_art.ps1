param(
    [string]$SourcePath = (Join-Path $PSScriptRoot "image.png"),
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
    $targetRatio = $width / [double]$height
    $sourceRatio = $source.Width / [double]$source.Height
    if ($sourceRatio -gt $targetRatio) {
        $cropHeight = $source.Height
        $cropWidth = [int][Math]::Round($cropHeight * $targetRatio)
        $cropX = [int](($source.Width - $cropWidth) / 2)
        $cropY = 0
    } else {
        $cropWidth = $source.Width
        $cropHeight = [int][Math]::Round($cropWidth / $targetRatio)
        $cropX = 0
        $cropY = [int](($source.Height - $cropHeight) / 2)
    }

    $graphics = [Drawing.Graphics]::FromImage($scaled)
    try {
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.DrawImage($source,
            (New-Object Drawing.Rectangle 0, 0, $width, $height),
            (New-Object Drawing.Rectangle $cropX, $cropY, $cropWidth, $cropHeight),
            [Drawing.GraphicsUnit]::Pixel)
    }
    finally {
        $graphics.Dispose()
    }

    $indices = New-Object 'byte[,]' $height, $width
    for ($y = 0; $y -lt $height; ++$y) {
        for ($x = 0; $x -lt $width; ++$x) {
            $color = $scaled.GetPixel($x, $y)
            $maximum = [Math]::Max($color.R, [Math]::Max($color.G, $color.B))
            $minimum = [Math]::Min($color.R, [Math]::Min($color.G, $color.B))
            $saturation = $maximum - $minimum
            $luminance = (30 * $color.R + 59 * $color.G + 11 * $color.B) / 100

            if ($luminance -lt 64) {
                $index = 0
            } elseif ($saturation -gt 42 -and
                      (($color.R - $color.G) -gt 12 -or ($color.B - $color.G) -gt 8)) {
                $index = 2
            } elseif ($luminance -ge 118) {
                $index = 1
            } else {
                $index = 0
            }
            $indices[$y, $x] = $index
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
            @(18, 104, 132, 0),
            @(212, 70, 184, 0),
            @(0, 0, 0, 0))) {
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

Write-Host "Converted splash artwork: $OutputPath"