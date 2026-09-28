<#
.SYNOPSIS
    Builds the Floor101 C++ targets with UnrealBuildTool (no Visual Studio IDE required).
.EXAMPLE
    .\Scripts\Build.ps1                      # Floor101Editor, Win64, Development
    .\Scripts\Build.ps1 -Target Game -Configuration Shipping
#>
[CmdletBinding()]
param(
    [ValidateSet('Editor', 'Game')]
    [string]$Target = 'Editor',
    [ValidateSet('Debug', 'DebugGame', 'Development', 'Shipping')]
    [string]$Configuration = 'Development'
)

. "$PSScriptRoot\Common.ps1"

$engine = Find-UnrealEngine
$targetName = if ($Target -eq 'Editor') { "${ProjectName}Editor" } else { $ProjectName }

# UBT refuses to build while an editor with Live Coding is running.
$editor = Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'" |
    Where-Object { $_.CommandLine -like "*$ProjectName.uproject*" } | Select-Object -First 1
if ($editor) {
    Write-Host ("Unreal Editor for $ProjectName is running (PID $($editor.ProcessId)). Close it, or compile from the " +
        "running editor with Live Coding (MCP: LiveCodingToolset.CompileLiveCoding).") -ForegroundColor Yellow
    exit 1
}

Write-Step "Unreal Engine $($engine.Version) (CL $($engine.Changelist)) from $($engine.Source): $($engine.Root)"
Write-Step "Building $targetName Win64 $Configuration"

$started = Get-Date
# No remote build helpers are used: keep Unreal Build Accelerator local instead of listening on 0.0.0.0.
# '-UBAHost=...' must stay quoted: PowerShell splits unquoted native args like -X=127.0.0.1 at the first '.'.
& $engine.BuildBat $targetName Win64 $Configuration "-Project=$UProjectPath" -WaitMutex -UBADisableRemote '-UBAHost=127.0.0.1' 2>&1 |
    Tee-Object -Variable buildOutput | Out-Host
$exitCode = $LASTEXITCODE

# Surface which MSVC toolchain and Windows SDK UnrealBuildTool actually selected.
$buildOutput | Select-String -Pattern 'toolchain|Windows .* SDK|preferred version' | ForEach-Object { Write-Step $_.Line.Trim() }

$elapsed = [int]((Get-Date) - $started).TotalSeconds
if ($exitCode -ne 0) {
    Write-Host "Build FAILED (exit $exitCode) after ${elapsed}s. Full UBT log: $env:LOCALAPPDATA\UnrealBuildTool\Log.txt" -ForegroundColor Red
    exit $exitCode
}
Write-Step "Build succeeded in ${elapsed}s"
