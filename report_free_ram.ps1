param(
    [Parameter(Mandatory = $true)]
    [string]$MapPath
)

$mainStart = 0x4000
$mainCapacity = 0x5C00
$basicRamStart = 0xA000
$basicRamCapacity = 0x2000
$mainUsed = 0
$basicRamUsed = 0
$segmentCount = 0
$inSegmentList = $false

foreach ($line in Get-Content $MapPath) {
    if ($line -eq "Segment list:") {
        $inSegmentList = $true
        continue
    }
    if (-not $inSegmentList) { continue }
    if ($line -eq "Exports list by name:") { break }
    if ($line -notmatch '^\s*\S+\s+([0-9A-Fa-f]{6})\s+[0-9A-Fa-f]{6}\s+([0-9A-Fa-f]{6})\s+') {
        continue
    }

    $start = [Convert]::ToInt32($Matches[1], 16)
    $size = [Convert]::ToInt32($Matches[2], 16)
    if ($start -ge $mainStart -and $start -lt $mainStart + $mainCapacity) {
        $mainUsed += $size
        ++$segmentCount
    }
    elseif ($start -ge $basicRamStart -and $start -lt $basicRamStart + $basicRamCapacity) {
        $basicRamUsed += $size
        ++$segmentCount
    }
}

if ($segmentCount -eq 0) {
    throw "Could not read segment usage from linker map: $MapPath"
}

$mainFree = $mainCapacity - $mainUsed
$basicRamFree = $basicRamCapacity - $basicRamUsed
$totalFree = $mainFree + $basicRamFree
Write-Host ("Remaining free RAM: {0} bytes (MAIN: {1}, BASICRAM: {2}; stack reserve: 1024)" -f `
    $totalFree, $mainFree, $basicRamFree)