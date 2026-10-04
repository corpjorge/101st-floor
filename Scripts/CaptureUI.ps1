<#
.SYNOPSIS
    Capture the actual application UI in a development build, then exit. Build first.
#>
[CmdletBinding()]
param(
    [string]$Output = 'Saved/UI/design.png',
    [ValidateSet('design', 'options', 'walk', 'photo', 'furniture', 'materials', 'home', 'grid', 'selection', 'search', 'favorites', 'materials-search')]
    [string]$Screen = 'design',
    [string]$Project = '',
    [int]$ResX = 1920,
    [int]$ResY = 1080
)
. "$PSScriptRoot/Common.ps1"
$uiEngine = Find-UnrealEngine
$uiCapturePath = [IO.Path]::GetFullPath((Join-Path $RepoRoot $Output))
if (-not $uiCapturePath.StartsWith($RepoRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Capture output must stay inside the repository.'
}
$uiArguments = @("`"$UProjectPath`"", '-game', '-DisablePython', '-windowed', "-ResX=$ResX", "-ResY=$ResY",
    '-ForceRes', '-unattended', '-nosplash', "-F101UICapture=`"$uiCapturePath`"", "-F101UIScreen=$Screen")
if ($Project) {
    $uiProjectPath = [IO.Path]::GetFullPath((Join-Path $RepoRoot $Project))
    if (-not (Test-Path -LiteralPath $uiProjectPath -PathType Leaf)) { throw "Project not found: $uiProjectPath" }
    $uiArguments += "-F101UIProject=`"$uiProjectPath`""
}
$uiProcess = Start-Process -FilePath $uiEngine.EditorExe -ArgumentList $uiArguments -WindowStyle Hidden -PassThru
if (-not $uiProcess.WaitForExit(180000)) { throw "Capture timed out; inspect process $($uiProcess.Id)." }
if ($uiProcess.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $uiCapturePath)) { throw 'UI capture failed; inspect Saved/Logs/Floor101.log.' }
Write-Output $uiCapturePath
