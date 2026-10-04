<#
.SYNOPSIS
    Check launcher diagnostic defaults, saved preferences and command-line overrides without launching Unreal.
#>
[CmdletBinding()]
param()
. "$PSScriptRoot/Common.ps1"
. "$PSScriptRoot/LaunchOptions.ps1"
$launcherChecksFolder = Join-Path $RepoRoot 'Saved/LauncherChecks'
New-Item -ItemType Directory -Force -Path $launcherChecksFolder | Out-Null
$launcherTestIni = Join-Path $launcherChecksFolder 'preferences.ini'
function Assert-Launcher($Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
try {
    $launcherDefault = @(Get-Floor101DiagnosticArguments -PreferencesPath '')
    Assert-Launcher ($launcherDefault.Count -eq 0) 'Normal startup must not request logs or debug.'
    [IO.File]::WriteAllText($launcherTestIni, "[Other]`nShowLogWindow=True`n[Floor101.Diagnostics]`nDebugMode=True`nShowLogWindow=False`n")
    $launcherDebug = @(Get-Floor101DiagnosticArguments -PreferencesPath $launcherTestIni)
    Assert-Launcher ($launcherDebug -contains '-F101Debug' -and $launcherDebug -notcontains '-log') 'Saved debug must not force a log window.'
    [IO.File]::WriteAllText($launcherTestIni, "[Floor101.Diagnostics]`nDebugMode=False`nShowLogWindow=True`n")
    $launcherWindow = @(Get-Floor101DiagnosticArguments -PreferencesPath $launcherTestIni)
    Assert-Launcher ($launcherWindow -contains '-log' -and $launcherWindow -notcontains '-F101Debug') 'Saved log window must not force debug.'
    $launcherQuiet = @(Get-Floor101DiagnosticArguments -PreferencesPath $launcherTestIni -Quiet)
    Assert-Launcher ($launcherQuiet.Count -eq 1 -and $launcherQuiet[0] -eq '-F101Quiet') 'Quiet must override saved diagnostics.'
    $launcherExplicit = @(Get-Floor101DiagnosticArguments -PreferencesPath '' -ShowLogs -DebugMode)
    Assert-Launcher ($launcherExplicit -contains '-log' -and $launcherExplicit -contains '-F101Debug') 'Explicit diagnostic recovery switches must work.'
    $launcherConflict = $false
    try { Get-Floor101DiagnosticArguments -Quiet -DebugMode | Out-Null } catch { $launcherConflict = $true }
    Assert-Launcher $launcherConflict 'Conflicting overrides must be rejected.'
    Assert-Launcher ($launcherQuiet -notcontains '-nolog' -and $launcherDefault -notcontains '-nolog') 'Hiding logs must preserve error files.'
    $launcherEntry = & "$PSScriptRoot/Run.ps1" -Quiet -DryRun -Demo:$false -Windowed
    Assert-Launcher ($launcherEntry.Arguments -contains '-F101Quiet' -and $launcherEntry.Arguments -notcontains '-log' -and $launcherEntry.Arguments -contains '-game') 'Run.ps1 must apply the launcher policy.'
    Write-Output 'Launcher checks passed.'
}
finally { Remove-Item -LiteralPath $launcherTestIni -ErrorAction SilentlyContinue }
