param(
    [Parameter(Mandatory = $true)]
    [string]$AtrPath,

    [Parameter(Mandatory = $true)]
    [hashtable]$Files,

    [hashtable]$RenameFiles = @{}
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

function Find-DosEntry([byte[]]$image, [string]$dosFileName) {
    $directoryName = Get-DosName $dosFileName
    for ($sector = 361; $sector -le 368; ++$sector) {
        $directoryOffset = Get-SectorOffset $sector
        for ($entry = 0; $entry -lt 8; ++$entry) {
            $candidate = $directoryOffset + ($entry * 16)
            if (($image[$candidate] -band 0x40) -eq 0) { continue }
            $candidateName = [System.Text.Encoding]::ASCII.GetString($image, $candidate + 5, 11)
            if ($candidateName -eq $directoryName) { return $candidate }
        }
    }
    return -1
}

function Test-SectorFree([byte[]]$image, [int]$sector) {
    $vtocOffset = Get-SectorOffset 360
    $bitmapOffset = $vtocOffset + 10 + [Math]::Floor($sector / 8)
    $mask = 0x80 -shr ($sector % 8)
    return ($image[$bitmapOffset] -band $mask) -ne 0
}

function Set-SectorAllocation([byte[]]$image, [int]$sector, [bool]$allocated) {
    $vtocOffset = Get-SectorOffset 360
    $bitmapOffset = $vtocOffset + 10 + [Math]::Floor($sector / 8)
    $mask = 0x80 -shr ($sector % 8)
    $currentlyFree = ($image[$bitmapOffset] -band $mask) -ne 0
    if ($allocated -eq (-not $currentlyFree)) { return }

    $freeSectors = $image[$vtocOffset + 3] + (256 * $image[$vtocOffset + 4])
    if ($allocated) {
        $image[$bitmapOffset] = $image[$bitmapOffset] -band (0xFF -bxor $mask)
        --$freeSectors
    } else {
        $image[$bitmapOffset] = $image[$bitmapOffset] -bor $mask
        ++$freeSectors
    }
    $image[$vtocOffset + 3] = $freeSectors -band 0xFF
    $image[$vtocOffset + 4] = ($freeSectors -shr 8) -band 0xFF
}

$resolvedAtr = (Resolve-Path $AtrPath).Path
$image = [System.IO.File]::ReadAllBytes($resolvedAtr)
if ($image.Length -ne 133136 -or $image[0] -ne 0x96 -or $image[1] -ne 0x02) {
    throw "'$AtrPath' is not a 1040-sector DOS 2.5 enhanced-density ATR."
}
if (($image[4] + (256 * $image[5])) -ne 128) {
    throw "'$AtrPath' does not use 128-byte sectors."
}

foreach ($oldDosFileName in $RenameFiles.Keys) {
    $newDosFileName = $RenameFiles[$oldDosFileName]
    $oldEntryOffset = Find-DosEntry $image $oldDosFileName
    $newEntryOffset = Find-DosEntry $image $newDosFileName
    if ($newEntryOffset -ge 0) { continue }
    if ($oldEntryOffset -lt 0) {
        throw "DOS file '$oldDosFileName' is not present in '$AtrPath'."
    }
    $newDirectoryName = Get-DosName $newDosFileName
    [System.Text.Encoding]::ASCII.GetBytes($newDirectoryName).CopyTo($image, $oldEntryOffset + 5)
}

foreach ($dosFileName in $Files.Keys) {
    $sourcePath = (Resolve-Path $Files[$dosFileName]).Path
    $entryOffset = Find-DosEntry $image $dosFileName

    if ($entryOffset -lt 0) {
        throw "DOS file '$dosFileName' is not present in '$AtrPath'."
    }

    $allocatedSectors = $image[$entryOffset + 1] + (256 * $image[$entryOffset + 2])
    $source = [System.IO.File]::ReadAllBytes($sourcePath)
    $requiredSectors = [int][Math]::Ceiling($source.Length / 125.0)
    if ($requiredSectors -eq 0) { $requiredSectors = 1 }

    $sector = $image[$entryOffset + 3] + (256 * $image[$entryOffset + 4])
    $visited = @{}
    $chain = New-Object System.Collections.Generic.List[int]
    for ($index = 0; $index -lt $allocatedSectors; ++$index) {
        if ($visited.ContainsKey($sector)) {
            throw "DOS file '$dosFileName' has a cyclic sector chain at sector $sector."
        }
        $visited[$sector] = $true
        $chain.Add($sector)
        $offset = Get-SectorOffset $sector
        $nextSector = (($image[$offset + 125] -band 0x03) * 256) + $image[$offset + 126]
        if ($index -eq $allocatedSectors - 1) {
            if ($nextSector -ne 0) {
                throw "DOS file '$dosFileName' has more sectors than its directory entry declares."
            }
        } elseif ($nextSector -eq 0) {
            throw "DOS file '$dosFileName' sector chain ends early at sector $sector."
        }
        $sector = $nextSector
    }

    if ($requiredSectors -gt $allocatedSectors) {
        $needed = $requiredSectors - $allocatedSectors
        for ($candidate = 1; $candidate -lt 720 -and $needed -gt 0; ++$candidate) {
            if (-not (Test-SectorFree $image $candidate)) { continue }
            Set-SectorAllocation $image $candidate $true
            $chain.Add($candidate)
            --$needed
        }
        if ($needed -ne 0) {
            throw "DOS file '$dosFileName' needs $needed more sectors, but the DOS 2.5 primary VTOC has no room."
        }
    } elseif ($requiredSectors -lt $allocatedSectors) {
        for ($index = $allocatedSectors - 1; $index -ge $requiredSectors; --$index) {
            $releasedSector = $chain[$index]
            Set-SectorAllocation $image $releasedSector $false
            [Array]::Clear($image, (Get-SectorOffset $releasedSector), 128)
            $chain.RemoveAt($index)
        }
    }

    $fileNumber = $image[(Get-SectorOffset $chain[0]) + 125] -band 0xFC
    $sourceOffset = 0
    for ($index = 0; $index -lt $requiredSectors; ++$index) {
        $sector = $chain[$index]
        $nextSector = if ($index + 1 -lt $requiredSectors) { $chain[$index + 1] } else { 0 }
        $offset = Get-SectorOffset $sector
        $chunkLength = [Math]::Min(125, $source.Length - $sourceOffset)
        [Array]::Clear($image, $offset, 125)
        if ($chunkLength -gt 0) {
            [Array]::Copy($source, $sourceOffset, $image, $offset, $chunkLength)
        }
        $image[$offset + 125] = $fileNumber -bor (($nextSector -shr 8) -band 0x03)
        $image[$offset + 126] = $nextSector -band 0xFF
        $image[$offset + 127] = $chunkLength
        $sourceOffset += $chunkLength
    }

    $image[$entryOffset + 1] = $requiredSectors -band 0xFF
    $image[$entryOffset + 2] = ($requiredSectors -shr 8) -band 0xFF
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