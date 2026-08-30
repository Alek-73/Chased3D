param(
    [Parameter(Mandatory = $true)]
    [string]$AtrPath,

    [Parameter(Mandatory = $true)]
    [hashtable]$Files
)

$ErrorActionPreference = "Stop"

function Get-SectorOffset([int]$sector) {
    if ($sector -lt 1 -or $sector -gt 1040) {
        throw "Invalid DOS 2.5 sector $sector."
    }
    return 16 + (($sector - 1) * 128)
}

function Get-DosName([string]$path) {
    $name = [System.IO.Path]::GetFileNameWithoutExtension($path).ToUpperInvariant()
    $extension = [System.IO.Path]::GetExtension($path).TrimStart('.').ToUpperInvariant()
    if ($name.Length -gt 8 -or $extension.Length -gt 3) {
        throw "'$path' does not fit an Atari DOS 8.3 filename."
    }
    return $name.PadRight(8) + $extension.PadRight(3)
}

$resolvedAtr = (Resolve-Path $AtrPath).Path
$image = [System.IO.File]::ReadAllBytes($resolvedAtr)
if ($image.Length -ne 133136 -or $image[0] -ne 0x96 -or $image[1] -ne 0x02) {
    throw "'$AtrPath' is not a 1040-sector DOS 2.5 enhanced-density ATR."
}
if (($image[4] + (256 * $image[5])) -ne 128) {
    throw "'$AtrPath' does not use 128-byte sectors."
}

foreach ($dosFileName in $Files.Keys) {
    $sourcePath = (Resolve-Path $Files[$dosFileName]).Path
    $directoryName = Get-DosName $dosFileName
    $entryOffset = -1

    for ($sector = 361; $sector -le 368 -and $entryOffset -lt 0; ++$sector) {
        $directoryOffset = Get-SectorOffset $sector
        for ($entry = 0; $entry -lt 8; ++$entry) {
            $candidate = $directoryOffset + ($entry * 16)
            if (($image[$candidate] -band 0x40) -eq 0) { continue }
            $candidateName = [System.Text.Encoding]::ASCII.GetString($image, $candidate + 5, 11)
            if ($candidateName -eq $directoryName) {
                $entryOffset = $candidate
                break
            }
        }
    }

    if ($entryOffset -lt 0) {
        throw "DOS file '$dosFileName' is not present in '$AtrPath'."
    }

    $allocatedSectors = $image[$entryOffset + 1] + (256 * $image[$entryOffset + 2])
    $source = [System.IO.File]::ReadAllBytes($sourcePath)
    $requiredSectors = [int][Math]::Ceiling($source.Length / 125.0)
    if ($requiredSectors -ne $allocatedSectors) {
        throw "DOS file '$dosFileName' needs $requiredSectors sectors but its existing chain has $allocatedSectors. Recreate the DOS 2.5 image with matching allocation before building."
    }

    $sector = $image[$entryOffset + 3] + (256 * $image[$entryOffset + 4])
    $visited = @{}
    $sourceOffset = 0
    for ($index = 0; $index -lt $allocatedSectors; ++$index) {
        if ($visited.ContainsKey($sector)) {
            throw "DOS file '$dosFileName' has a cyclic sector chain at sector $sector."
        }
        $visited[$sector] = $true

        $offset = Get-SectorOffset $sector
        $nextSector = (($image[$offset + 125] -band 0x03) * 256) + $image[$offset + 126]
        $chunkLength = [Math]::Min(125, $source.Length - $sourceOffset)
        [Array]::Clear($image, $offset, 125)
        [Array]::Copy($source, $sourceOffset, $image, $offset, $chunkLength)
        $image[$offset + 127] = $chunkLength
        $sourceOffset += $chunkLength

        if ($index -eq $allocatedSectors - 1) {
            if ($nextSector -ne 0) {
                throw "DOS file '$dosFileName' has more sectors than its directory entry declares."
            }
        } elseif ($nextSector -eq 0) {
            throw "DOS file '$dosFileName' sector chain ends early at sector $sector."
        }
        $sector = $nextSector
    }
}

$temporaryPath = "$resolvedAtr.tmp"
try {
    [System.IO.File]::WriteAllBytes($temporaryPath, $image)
    Move-Item -Force $temporaryPath $resolvedAtr
}
finally {
    if (Test-Path $temporaryPath) { Remove-Item $temporaryPath }
}

Write-Host "Updated DOS 2.5 image: $resolvedAtr"