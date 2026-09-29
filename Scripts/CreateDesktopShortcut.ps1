<#
.SYNOPSIS
    Creates (or refreshes) a "101st Floor" shortcut on the desktop that runs .\Scripts\Run.ps1.
.DESCRIPTION
    For quick manual testing: the shortcut starts the game window (editor binary in -game mode, as Run.ps1 does) through
    PowerShell 7 in a minimized window. Build first with .\Scripts\Build.ps1. The desktop folder is resolved by Windows,
    so a OneDrive-redirected desktop works.
.EXAMPLE
    .\Scripts\CreateDesktopShortcut.ps1
    .\Scripts\CreateDesktopShortcut.ps1 -Demo:$false
#>
[CmdletBinding()]
param(
    # Load the MVP 0 reference wall at start (Run.ps1 -Demo).
    [bool]$Demo = $true
)

. "$PSScriptRoot\Common.ps1"

$engine = Find-UnrealEngine
# A Store install lives under a versioned WindowsApps folder; its app execution alias survives updates.
$alias = Join-Path $env:LOCALAPPDATA 'Microsoft\WindowsApps\pwsh.exe'
$pwsh = if (Test-Path $alias) { $alias } else { (Get-Command pwsh -ErrorAction Stop).Source }
$runScript = Join-Path $PSScriptRoot 'Run.ps1'
$desktop = [Environment]::GetFolderPath('Desktop')
$path = Join-Path $desktop '101st Floor.lnk'

$arguments = "-NoProfile -ExecutionPolicy Bypass -File `"$runScript`""
if (-not $Demo) { $arguments += ' -Demo:$false' }

$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($path)
$shortcut.TargetPath = $pwsh
$shortcut.Arguments = $arguments
$shortcut.WorkingDirectory = Split-Path $PSScriptRoot -Parent
$shortcut.IconLocation = "$($engine.EditorExe),0"
$shortcut.WindowStyle = 7 # minimized: the PowerShell window only launches the game
$shortcut.Description = 'Abre 101st Floor (Scripts\Run.ps1)'
$shortcut.Save()

Write-Step "Shortcut: $path -> $pwsh $arguments"
