<#
.SYNOPSIS
    Deletes regenerable build output: Binaries, Intermediate (UBT, UHT and unit test builds), DerivedDataCache.
.DESCRIPTION
    Saved (logs, screenshots, automation reports, per-user editor settings) is kept unless -IncludeSaved is given.
    Never touches versioned files. Refuses to run while the editor has the project open.
.EXAMPLE
    .\Scripts\Clean.ps1
    .\Scripts\Clean.ps1 -IncludeSaved
#>
[CmdletBinding(SupportsShouldProcess)]
param(
    [switch]$IncludeSaved
)

. "$PSScriptRoot\Common.ps1"

$editor = Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe' OR Name='UnrealEditor-Cmd.exe'" |
    Where-Object { $_.CommandLine -like "*$ProjectName.uproject*" } | Select-Object -First 1
if ($editor) { throw "Unreal Editor has $ProjectName open (PID $($editor.ProcessId)); close it first." }

$targets = 'Binaries', 'Intermediate', 'DerivedDataCache'
if ($IncludeSaved) { $targets += 'Saved' }
foreach ($name in $targets) {
    $path = Join-Path $RepoRoot $name
    if ((Test-Path $path) -and $PSCmdlet.ShouldProcess($path, 'Remove')) {
        Remove-Item -Recurse -Force $path
        Write-Step "Removed $name"
    }
}
