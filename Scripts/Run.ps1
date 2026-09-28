<#
.SYNOPSIS
    Runs 101st Floor (standalone game window, no editor UI). Build first with .\Scripts\Build.ps1.
.DESCRIPTION
    Until there is a packaged build, the app runs from the editor binary in -game mode. With the default -Demo the
    MVP 0 reference wall is loaded; the f101.Demo.* console commands (e.g. "f101.Demo.SetLength 4.2 m") edit it.
.EXAMPLE
    .\Scripts\Run.ps1
    .\Scripts\Run.ps1 -Demo:$false -ResX 2560 -ResY 1440
#>
[CmdletBinding()]
param(
    [bool]$Demo = $true,
    [int]$ResX = 1920,
    [int]$ResY = 1080
)

. "$PSScriptRoot\Common.ps1"

$engine = Find-UnrealEngine
$arguments = @("`"$UProjectPath`"", '-game', '-windowed', "-ResX=$ResX", "-ResY=$ResY", '-log')
if ($Demo) { $arguments += '-F101Demo' }

Write-Step "Starting 101st Floor ($($engine.Version)) $(if ($Demo) { 'with the MVP 0 reference wall' })"
Start-Process -FilePath $engine.EditorExe -ArgumentList $arguments | Out-Null
