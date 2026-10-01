<#
.SYNOPSIS
    Second opinion on an exported .glb (ADR-0020): imports it into Blender (headless), prints what Blender read and renders
    a picture of it.
.DESCRIPTION
    Needs Blender 4.2 or later (the newest under "C:\Program Files\Blender Foundation" is used, or -Blender). The picture
    goes next to the file as <name>-blender.png unless -Picture says where.
.EXAMPLE
    .\Scripts\CheckGltf.ps1 -Path "$env:USERPROFILE\Documents\101st Floor\project.glb"
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Path,
    [string]$Picture,
    [string]$Blender
)

. "$PSScriptRoot\Common.ps1"

$Path = (Resolve-Path $Path).Path
if (-not $Picture) { $Picture = [IO.Path]::ChangeExtension($Path, $null).TrimEnd('.') + '-blender.png' }
if (-not $Blender) {
    $Blender = Get-ChildItem 'C:\Program Files\Blender Foundation' -Directory -ErrorAction SilentlyContinue |
        Sort-Object { [version]($_.Name -replace '[^\d.]', '') } -Descending |
        ForEach-Object { Join-Path $_.FullName 'blender.exe' } | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $Blender -or -not (Test-Path $Blender)) {
    throw 'Blender not found: install it under "C:\Program Files\Blender Foundation" or pass -Blender <blender.exe>.'
}

$script = Join-Path $RepoRoot 'Tests\Tools\check_gltf.py'
$logPath = Join-Path $RepoRoot 'Saved\Logs\CheckGltf.log'
New-Item -ItemType Directory -Force (Split-Path $logPath) | Out-Null
Write-Step "Importing $Path into $Blender"
$arguments = @('-b', '--factory-startup', '--python', "`"$script`"", '--', "`"$Path`"", "`"$Picture`"")
$process = Start-Process -FilePath $Blender -ArgumentList $arguments -PassThru -NoNewWindow -Wait -RedirectStandardOutput $logPath `
    -RedirectStandardError "$logPath.stderr.txt"
$lines = @(Select-String -Path $logPath -Pattern '^F101GLTF (.+)$' | ForEach-Object { $_.Matches[0].Groups[1].Value })
$errors = @(Select-String -Path $logPath, "$logPath.stderr.txt" -Pattern 'Traceback|Error:' -ErrorAction SilentlyContinue)
$lines | ForEach-Object { Write-Host "  $_" }
if ($process.ExitCode -ne 0 -or $errors.Count -gt 0 -or -not ($lines -match '^picture ')) {
    $errors | ForEach-Object { Write-Host "  $($_.Line)" }
    throw "Blender could not import or render the file (exit $($process.ExitCode)). Log: $logPath"
}
Write-Step "Blender read it: picture in $Picture"
