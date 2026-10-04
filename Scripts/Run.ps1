<#
.SYNOPSIS
    Runs 101st Floor (standalone game window, no editor UI). Build first with .\Scripts\Build.ps1.
.DESCRIPTION
    Until there is a packaged build, the app runs from the editor binary in -game mode. With the default -Demo the
    MVP 0 reference wall is loaded; the f101.Demo.* console commands (e.g. "f101.Demo.SetLength 4.2 m") edit it.
.EXAMPLE
    .\Scripts\Run.ps1
    .\Scripts\Run.ps1 -Demo:$false -ResX 2560 -ResY 1440
    .\Scripts\Run.ps1 -Fullscreen
    .\Scripts\Run.ps1 -DebugMode -ShowLogs
    .\Scripts\Run.ps1 -Quiet
#>
[CmdletBinding()]
param(
    [bool]$Demo = $true,
    [int]$ResX = 1920,
    [int]$ResY = 1080,
    [switch]$Windowed,
    [switch]$Fullscreen,
    [switch]$DebugMode,
    [switch]$ShowLogs,
    [switch]$Quiet,
    [switch]$DryRun
)

. "$PSScriptRoot\Common.ps1"
. "$PSScriptRoot/LaunchOptions.ps1"

$engine = Find-UnrealEngine
if ($Windowed -and $Fullscreen) { throw 'Choose either -Windowed or -Fullscreen.' }
# -DisablePython: the app has no Python. Without it the editor's Python plugin still runs the init_unreal.py of
# editor-only plugins (ToolsetRegistry), whose classes are not loaded in -game, and logs a traceback at startup.
$arguments = @("`"$UProjectPath`"", '-game', '-DisablePython')
$launchPreferences = @('WindowsEditor', 'Windows' | ForEach-Object { Join-Path $RepoRoot "Saved/Config/$_/GameUserSettings.ini" } | Where-Object { Test-Path -LiteralPath $_ }) | Select-Object -First 1
$arguments += @(Get-Floor101DiagnosticArguments -PreferencesPath $launchPreferences -DebugMode:$DebugMode -ShowLogs:$ShowLogs -Quiet:$Quiet)
# Honor the display mode and resolution saved by the UI. Retain the initial 1920x1080 window on a fresh install.
$hasDisplayPreferences = @('WindowsEditor', 'Windows' | Where-Object {
    Test-Path -LiteralPath (Join-Path $RepoRoot "Saved\Config\$_\GameUserSettings.ini")
}).Count -gt 0
if ($Fullscreen) { $arguments += '-fullscreen' }
elseif ($Windowed -or -not $hasDisplayPreferences) { $arguments += '-windowed' }
if ($PSBoundParameters.ContainsKey('ResX') -or $PSBoundParameters.ContainsKey('ResY') -or (-not $hasDisplayPreferences -and -not $Fullscreen)) {
    $arguments += "-ResX=$ResX", "-ResY=$ResY"
}
if ($Demo) { $arguments += '-F101Demo' }

if ($DryRun) { return [pscustomobject]@{ FilePath = $engine.EditorExe; Arguments = $arguments } }

Write-Step "Starting 101st Floor ($($engine.Version)) $(if ($Demo) { 'with the MVP 0 reference wall' })"
Start-Process -FilePath $engine.EditorExe -ArgumentList $arguments -WindowStyle Hidden | Out-Null
