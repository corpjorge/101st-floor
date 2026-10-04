<#
.SYNOPSIS
    Regenerates the project's assets from code (Content/Floor101/...), so Content/ is reproducible from Git + Scripts.
.DESCRIPTION
    Runs the Floor101GenerateAssets commandlet in a headless editor. Today it builds the material every finish uses
    (ADR-0012): /Game/Floor101/Materials/M_Floor101Finish. Build first (.\Scripts\Build.ps1) with the editor closed.
    Commit the regenerated .uasset (Git LFS).
.EXAMPLE
    .\Scripts\GenerateAssets.ps1
#>
[CmdletBinding()]
param()

. "$PSScriptRoot\Common.ps1"

$engine = Find-UnrealEngine
$logPath = Join-Path $RepoRoot 'Saved\Logs\GenerateAssets.log'
$arguments = @("`"$UProjectPath`"", '-run=Floor101GenerateAssets', '-unattended', '-nopause', '-nosplash', '-nullrhi',
    "-abslog=`"$logPath`"")

Write-Step "Generating assets with $($engine.EditorCmdExe)"
$process = Start-Process -FilePath $engine.EditorCmdExe -ArgumentList $arguments -PassThru -WindowStyle Hidden -Wait `
    -RedirectStandardOutput "$logPath.stdout.txt"
$saved = Select-String -Path $logPath -Pattern 'LogFloor101Assets: Display: Saved (.+)$' -ErrorAction SilentlyContinue
if ($process.ExitCode -ne 0 -or -not $saved) {
    throw "Asset generation failed (exit $($process.ExitCode)). Log: $logPath"
}
foreach ($line in $saved) {
    Write-Step "Saved $($line.Matches[0].Groups[1].Value)"
}
