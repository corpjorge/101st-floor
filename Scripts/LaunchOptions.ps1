# Launcher policy shared with the executable checks; importing this file never starts the game.
function Get-Floor101DiagnosticArguments {
    param([string]$PreferencesPath, [switch]$DebugMode, [switch]$ShowLogs, [switch]$Quiet)
    if ($Quiet -and ($DebugMode -or $ShowLogs)) { throw 'Quiet cannot be combined with DebugMode or ShowLogs.' }
    $launchDebug = $false
    $launchShowLogs = $false
    if ($PreferencesPath -and (Test-Path -LiteralPath $PreferencesPath)) {
        $launchInSection = $false
        foreach ($launchLine in Get-Content -LiteralPath $PreferencesPath) {
            if ($launchLine -match '^\s*\[([^]]+)\]') { $launchInSection = $Matches[1] -eq 'Floor101.Diagnostics'; continue }
            if ($launchInSection -and $launchLine -match '^\s*(DebugMode|ShowLogWindow)\s*=\s*(True|False|1|0)\s*(?:[;#].*)?$') {
                $launchValue = $Matches[2] -in 'True', '1'
                if ($Matches[1] -eq 'DebugMode') { $launchDebug = $launchValue } else { $launchShowLogs = $launchValue }
            }
        }
    }
    if ($Quiet) { return @('-F101Quiet') }
    if ($launchDebug -or $DebugMode) { '-F101Debug'; '-LogCmds="LogFloor101 VeryVerbose"' }
    if ($launchShowLogs -or $ShowLogs) { '-log' }
}
