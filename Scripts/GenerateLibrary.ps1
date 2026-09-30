<#
.SYNOPSIS
    Regenerates the furniture of the built-in library (ADR-0019) from code: Library/furniture/*.glb, their pictures and
    manifest.json, modelled by Library/Source/make_furniture.py in Blender (headless).
.DESCRIPTION
    Needs Blender 4.2 or later (the newest under "C:\Program Files\Blender Foundation" is used, or -Blender). The
    textures of the library (Library/textures) are not generated: they are CC0 images listed in their own manifest.
    Commit the regenerated files (the .glb and .png go through Git LFS).
.EXAMPLE
    .\Scripts\GenerateLibrary.ps1
    .\Scripts\GenerateLibrary.ps1 -Only chair-windsor, sofa-corner
#>
[CmdletBinding()]
param(
    [string]$Blender,
    # Only these pieces (ids); the manifest still lists them all.
    [string[]]$Only = @()
)

. "$PSScriptRoot\Common.ps1"

if (-not $Blender) {
    $Blender = Get-ChildItem 'C:\Program Files\Blender Foundation' -Directory -ErrorAction SilentlyContinue |
        Sort-Object { [version]($_.Name -replace '[^\d.]', '') } -Descending |
        ForEach-Object { Join-Path $_.FullName 'blender.exe' } | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $Blender -or -not (Test-Path $Blender)) {
    throw 'Blender not found: install it under "C:\Program Files\Blender Foundation" or pass -Blender <blender.exe>.'
}

$script = Join-Path $RepoRoot 'Library\Source\make_furniture.py'
$logPath = Join-Path $RepoRoot 'Saved\Logs\GenerateLibrary.log'
New-Item -ItemType Directory -Force (Split-Path $logPath) | Out-Null
$arguments = @('-b', '--factory-startup', '--python', "`"$script`"")
if ($Only.Count -gt 0) { $arguments += @('--') + $Only }

Write-Step "Modelling the library furniture with $Blender"
$started = Get-Date
$process = Start-Process -FilePath $Blender -ArgumentList $arguments -PassThru -NoNewWindow -Wait -RedirectStandardOutput $logPath `
    -RedirectStandardError "$logPath.stderr.txt"
$pieces = @(Select-String -Path $logPath -Pattern '^F101LIB (.+)$' | ForEach-Object { $_.Matches[0].Groups[1].Value })
$errors = @(Select-String -Path $logPath, "$logPath.stderr.txt" -Pattern 'Traceback|Error:' -ErrorAction SilentlyContinue)
if ($process.ExitCode -ne 0 -or $errors.Count -gt 0 -or $pieces.Count -eq 0) {
    throw "Library generation failed (exit $($process.ExitCode)). Log: $logPath"
}
$pieces | ForEach-Object { Write-Host "  $_" }
Write-Step ("{0} pieces in {1:N0}s -> Library\furniture" -f $pieces.Count, ((Get-Date) - $started).TotalSeconds)
