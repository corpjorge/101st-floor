<#
.SYNOPSIS
    Exercise the real native splash and PreLoadScreen handoff, then capture the shared loading component.
#>
[CmdletBinding()]
param(
    [switch]$DebugMode,
    [ValidateRange(720,7680)][int]$ResX = 1280,
    [ValidateRange(420,4320)][int]$ResY = 720
)
. "$PSScriptRoot/Common.ps1"
$startupEngine = Find-UnrealEngine
$startupMode = if ($DebugMode) { 'debug' } else { 'normal' }
$startupReportFolder = Join-Path $RepoRoot ('Saved/StartupChecks/' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + $startupMode)
New-Item -ItemType Directory -Force -Path $startupReportFolder | Out-Null
$startupLog = Join-Path $startupReportFolder 'Startup.log'
$startupCapture = Join-Path $startupReportFolder 'loading.png'
$startupArguments = @("`"$UProjectPath`"", '-game', '-DisablePython', '-windowed', "-ResX=$ResX", "-ResY=$ResY", '-ForceRes', '-unattended',
    "-abslog=`"$startupLog`"", "-F101UICapture=`"$startupCapture`"", '-F101UIScreen=startup')
# No -nosplash or -NoLoadingScreen: this must exercise the real loading sequence.
if ($DebugMode) { $startupArguments += '-F101Debug', '-LogCmds="LogFloor101 VeryVerbose"' }
else { $startupArguments += '-F101Quiet' }
$startupProcess = Start-Process -FilePath $startupEngine.EditorExe -ArgumentList $startupArguments -WindowStyle Hidden -PassThru
if (-not $startupProcess.WaitForExit(180000)) { throw "Startup timed out; inspect process $($startupProcess.Id)." }
if ($startupProcess.ExitCode -ne 0) { throw "Startup failed: $startupLog" }
$startupText = Get-Content -LiteralPath $startupLog -Raw
foreach ($startupMarker in @('F101: branded loading screen shown', 'F101: branded loading screen finished', "F101: diagnostics=$(if ($DebugMode) { 'on' } else { 'off' }) log window=off")) {
    if (-not $startupText.Contains($startupMarker)) { throw "Missing startup marker '$startupMarker': $startupLog" }
}
if (-not (Test-Path -LiteralPath $startupCapture)) { throw 'Loading component capture was not written.' }
# Detect missing logo resources as well as a successful engine handoff.
Add-Type -AssemblyName System.Drawing
$startupBitmap = [Drawing.Bitmap]::new($startupCapture)
try {
    $startupGold = 0
    $startupLogoLeft = [int]($startupBitmap.Width / 2 - 48)
    $startupLogoTop = [int]($startupBitmap.Height / 2 - 134)
    for ($startupY = $startupLogoTop; $startupY -lt $startupLogoTop + 96; $startupY++) {
        for ($startupX = $startupLogoLeft; $startupX -lt $startupLogoLeft + 96; $startupX++) {
            $startupPixel = $startupBitmap.GetPixel($startupX, $startupY)
            if ($startupPixel.R -gt 170 -and $startupPixel.G -gt 125 -and $startupPixel.B -lt 155) { $startupGold++ }
        }
    }
    if ($startupGold -lt 800) { throw "Startup logo is missing from the rendered capture: $startupCapture" }
}
finally { $startupBitmap.Dispose() }
Write-Output "Startup $startupMode passed: $startupReportFolder"
