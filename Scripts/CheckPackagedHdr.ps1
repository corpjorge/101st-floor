<#
.SYNOPSIS
    Exercise HDR in the Shipping executable, optionally enabling Windows HDR temporarily and restoring it.
#>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Package, [switch]$Display, [switch]$EnableWindowsHdrForTest)
. "$PSScriptRoot/Common.ps1"
$null = & "$PSScriptRoot/CheckHdr.ps1"
$packagedHdrOriginal = [Floor101.DisplayProbe]::ReadWindows()
$packagedHdrChanged = [System.Collections.Generic.List[object]]::new()
try {
    if ($EnableWindowsHdrForTest) {
        foreach ($packagedHdrState in $packagedHdrOriginal | Where-Object { $_.Supported -and -not $_.Enabled }) {
            $packagedHdrChanged.Add($packagedHdrState)
            [Floor101.DisplayProbe]::SetWindows($packagedHdrState, $true)
        }
        Start-Sleep -Seconds 3
        if (-not ([Floor101.DisplayProbe]::ReadWindows() | Where-Object Active)) { throw 'Windows did not activate HDR.' }
    }
    $packagedHdrCheck = if ($Display) { 'display' } else { 'hdr' }
    $packagedHdrOutput = & "$PSScriptRoot/CheckPackage.ps1" -Package $Package -Check $packagedHdrCheck
    $packagedHdrDirectory = @($packagedHdrOutput)[-1]
    if ($EnableWindowsHdrForTest -and (Get-Content -LiteralPath (Join-Path $packagedHdrDirectory 'Floor101.log') -Raw) -notmatch 'hdr_on=1') {
        throw 'HDR activation was not demonstrated in Shipping.'
    }
    Write-Output $packagedHdrDirectory
}
finally {
    foreach ($packagedHdrState in $packagedHdrChanged) { [Floor101.DisplayProbe]::SetWindows($packagedHdrState, $packagedHdrState.Enabled) }
    Write-Step 'Original Windows HDR state restored; application test preferences are isolated.'
}
