<#
.SYNOPSIS
    Validates the real application's HDR toggle, viewport/window state, OS luminance, EXR and SDR capture.
.DESCRIPTION
    Default: observes the current Windows HDR state. -EnableWindowsHdrForTest temporarily enables HDR on supported
    displays and restores their original state in finally. Application preferences are also restored.
    -Display additionally checks native fullscreen, VSync on/off preserving HDR, a lower supported resolution,
    return to the previous window and graceful application exit through the Salir action.
.EXAMPLE
    .\Scripts\CheckHdrOutput.ps1 -EnableWindowsHdrForTest
    .\Scripts\CheckHdrOutput.ps1 -Display -EnableWindowsHdrForTest
#>
[CmdletBinding()]
param([switch]$EnableWindowsHdrForTest, [switch]$Display)

. "$PSScriptRoot\Common.ps1"
$engine = Find-UnrealEngine
$null = & "$PSScriptRoot\CheckHdr.ps1"
$original = [Floor101.DisplayProbe]::ReadWindows()
$changed = [System.Collections.Generic.List[object]]::new()
$configBackups = @('WindowsEditor', 'Windows' | ForEach-Object {
    $path = Join-Path $RepoRoot "Saved\Config\$_\GameUserSettings.ini"
    [pscustomobject]@{ Path = $path; Existed = (Test-Path -LiteralPath $path); Bytes = $(if (Test-Path -LiteralPath $path) { [IO.File]::ReadAllBytes($path) }) }
})
$process = $null
$runLog = Join-Path $RepoRoot ("Saved\HDR\Output-{0:yyyyMMdd-HHmmss}.log" -f (Get-Date))
$checkName = if ($Display) { 'Display/HDR' } else { 'HDR' }
try {
    if ($EnableWindowsHdrForTest) {
        foreach ($displayState in $original | Where-Object { $_.Supported -and -not $_.Enabled }) {
            $changed.Add($displayState)
            [Floor101.DisplayProbe]::SetWindows($displayState, $true)
        }
        Start-Sleep -Seconds 3
        $null = & "$PSScriptRoot\CheckHdr.ps1" -ReportPath (Join-Path $RepoRoot 'Saved\HDR\Monitor-HDR.json')
        if (-not ([Floor101.DisplayProbe]::ReadWindows() | Where-Object Active)) { throw 'Windows did not activate HDR.' }
    }
    $checkFlag = if ($Display) { '-F101DisplayCheck' } else { '-F101HDRCheck' }
    $arguments = @("`"$UProjectPath`"", '-game', '-windowed', '-ResX=1280', '-ResY=720', '-WinX=64', '-WinY=64',
        '-F101Demo', $checkFlag, '-unattended', '-nosplash', '-nosound', '-DisablePython', '-log', "-abslog=`"$runLog`"")
    Write-Step "$checkName runtime check (log: $runLog)"
    $process = Start-Process -FilePath $engine.EditorExe -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit(300000)) { throw "$checkName runtime check timed out." }
    $log = Get-Content -LiteralPath $runLog -Raw
    $log -split "`n" | Where-Object { $_ -match 'F101HDRCheck:|F101HDR:' } | ForEach-Object { Write-Host $_.Trim() }
    if ($process.ExitCode -ne 0 -or $log -notmatch 'F101HDRCheck: result=PASS') { throw "$checkName runtime check failed." }
    if ($EnableWindowsHdrForTest -and $log -notmatch 'F101HDRCheck: hdr_on=1') { throw 'HDR activation was not demonstrated.' }
    if ($log -match 'Log(?:D3D12RHI|RHI|Renderer|Material|Shader)[^:]*: (?:Error|Fatal):') { throw 'Rendering errors in the HDR run.' }
    Write-Step "$checkName output check passed"
}
finally {
    if ($process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force }
    foreach ($displayState in $changed) { [Floor101.DisplayProbe]::SetWindows($displayState, $displayState.Enabled) }
    foreach ($config in $configBackups) {
        if ($config.Existed) { [IO.File]::WriteAllBytes($config.Path, $config.Bytes) }
        elseif (Test-Path -LiteralPath $config.Path) { Remove-Item -LiteralPath $config.Path }
    }
    $null = & "$PSScriptRoot\CheckHdr.ps1" -ReportPath (Join-Path $RepoRoot 'Saved\HDR\Monitor-Restored.json')
    Write-Step 'Original Windows HDR state and application preferences restored'
}
