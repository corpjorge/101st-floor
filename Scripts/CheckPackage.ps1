<#
.SYNOPSIS
    Verify a portable release and exercise its actual Shipping executable and product APIs.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Package,
    [ValidateSet('functional', 'home', 'startup', 'diagnostics', 'display', 'hdr', 'doors', 'windows', 'update')][string]$Check = 'functional',
    [int]$ResX = 1280, [int]$ResY = 720,
    # -Check update (ADR-0039): the release to read (GitHub when empty, or the local-release.json Release.ps1 writes) and
    # the version this executable pretends to be, so that release is newer. It downloads and verifies; it never installs.
    [string]$UpdateFeed = '', [string]$UpdateFrom = '0.0.1'
)
. "$PSScriptRoot/Common.ps1"
$checkPackage = [IO.Path]::GetFullPath((Join-Path $RepoRoot $Package))
if (-not $checkPackage.StartsWith([IO.Path]::GetFullPath((Join-Path $RepoRoot 'Packaged')) + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Validation input must stay under Packaged/.' }
$checkManifest = Get-Content -LiteralPath (Join-Path $checkPackage 'release-manifest.json') -Raw | ConvertFrom-Json
foreach ($checkFile in $checkManifest.files) {
    $checkPath = [IO.Path]::GetFullPath((Join-Path $checkPackage $checkFile.path))
    if (-not $checkPath.StartsWith($checkPackage + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe manifest path.' }
    if ((Get-FileHash -LiteralPath $checkPath -Algorithm SHA256).Hash -ne $checkFile.sha256) { throw "Invalid release file: $($checkFile.path)" }
}
$checkUnexpected = @(Get-ChildItem -LiteralPath $checkPackage -File -Recurse | Where-Object { $_.Name -match 'UnrealEditor|\.pdb$|\.uproject$' })
if ($checkUnexpected) { throw 'The package contains editor or development files.' }
$checkDirectory = Join-Path $RepoRoot "Saved/PackageChecks/$(Get-Date -Format yyyyMMdd-HHmmss)-$Check"
New-Item -ItemType Directory -Path $checkDirectory | Out-Null
$checkLog = Join-Path $checkDirectory 'Floor101.log'
$checkArguments = @('-DX12', '-windowed', "-ResX=$ResX", "-ResY=$ResY", '-ForceRes', '-unattended', '-F101Quiet',
    "-abslog=`"$checkLog`"", "-UserDir=`"$checkDirectory/User/`"")
if ($Check -eq 'functional') { $checkArguments += '-F101PackageCheck', "-F101PackageOutput=`"$checkDirectory`"" }
elseif ($Check -eq 'display') { $checkArguments += '-F101DisplayCheck', '-F101Demo' }
elseif ($Check -eq 'hdr') { $checkArguments += '-F101HDRCheck', '-F101Demo' }
elseif ($Check -eq 'update') {
    $checkArguments += '-F101UpdateCheck', '-F101UpdateDryRun', "-F101UpdateVersion=$UpdateFrom"
    if ($UpdateFeed) { $checkArguments += "-F101UpdateFeed=`"$([IO.Path]::GetFullPath((Join-Path $RepoRoot $UpdateFeed)))`"" }
}
else { $checkArguments += "-F101UICapture=`"$(Join-Path $checkDirectory "$Check.png")`"", "-F101UIScreen=$Check" }
# Test the same bootstrap executable that the user opens, not UnrealEditor -game.
$checkLaunch = Start-Process -FilePath (Join-Path $checkPackage 'Floor101.exe') -ArgumentList $checkArguments -WorkingDirectory $checkPackage -WindowStyle Hidden -PassThru
$checkProcess = $null
$checkWaitUntil = (Get-Date).AddSeconds(60)
while (-not $checkProcess -and (Get-Date) -lt $checkWaitUntil) {
    $checkChild = Get-CimInstance Win32_Process -Filter "Name='Floor101-Win64-Shipping.exe' OR Name='Floor101.exe'" |
        Where-Object { $_.ExecutablePath -and $_.ExecutablePath.StartsWith($checkPackage + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -and $_.ParentProcessId -eq $checkLaunch.Id } | Select-Object -First 1
    if ($checkChild) { $checkProcess = Get-Process -Id $checkChild.ProcessId; $null = $checkProcess.Handle }
    else { Start-Sleep -Milliseconds 500 }
}
if (-not $checkProcess) { throw "No standalone game started; see $checkDirectory" }
try {
    $checkModules = @($checkProcess.Modules | ForEach-Object FileName)
    $checkModules | Set-Content -LiteralPath (Join-Path $checkDirectory 'loaded-modules.txt') -Encoding utf8
    if ($checkModules | Where-Object { $_ -match 'Epic Games[/\\]UE_|UnrealEditor' }) { throw 'The running game loaded a development engine dependency.' }
    if (-not $checkProcess.WaitForExit($(if ($Check -eq 'update') { 1200000 } else { 300000 }))) { throw 'Packaged application check timed out.' }
    if ($checkProcess.ExitCode -ne 0) { throw "Packaged application exited with $($checkProcess.ExitCode)." }
}
finally { if (-not $checkProcess.HasExited) { Stop-Process -Id $checkProcess.Id } }
if (-not (Test-Path -LiteralPath $checkLog)) { throw 'Shipping product logging is missing.' }
$checkText = Get-Content -LiteralPath $checkLog -Raw
if ($Check -eq 'functional') {
    if ($checkText -notmatch 'F101PackageCheck: result=PASS' -or $checkText -match 'F101PackageCheck: \w+=FAIL') { throw "Functional checks failed: $checkLog" }
    foreach ($checkArtifact in 'project.json', 'plan.svg', 'plan.png', 'scene.glb', 'design.png', 'photo.png') {
        if (-not (Test-Path -LiteralPath (Join-Path $checkDirectory $checkArtifact))) { throw "Missing check artifact: $checkArtifact" }
    }
}
elseif ($Check -in 'display','hdr') {
    if ($checkText -notmatch 'F101HDRCheck: result=PASS') { throw "Display/HDR check failed: $checkLog" }
}
elseif ($Check -eq 'update') {
    if ($checkText -notmatch 'F101UpdateCheck: ready result=PASS' -or $checkText -notmatch 'F101Update: dry run, would run cmd\.exe') { throw "Update check failed: $checkLog" }
}
elseif (-not (Test-Path -LiteralPath (Join-Path $checkDirectory "$Check.png"))) { throw 'Shipping UI capture failed.' }
if ($checkText -notmatch 'branded loading screen shown' -or $checkText -notmatch 'branded loading screen finished') { throw 'The actual branded loading phase was not observed.' }
Write-Step "PASS: $Check; standalone executable, release hashes and local dependencies checked."
Write-Output $checkDirectory
