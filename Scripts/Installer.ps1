<#
.SYNOPSIS
    Create a Windows setup EXE and MSI from a verified portable package. WiX stays local.
#>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Package, [switch]$RestoreTools)
. "$PSScriptRoot/Common.ps1"
$installerPackage = [IO.Path]::GetFullPath((Join-Path $RepoRoot $Package))
$installerReleaseRoot = [IO.Path]::GetFullPath((Join-Path $RepoRoot 'Packaged')) + [IO.Path]::DirectorySeparatorChar
if (-not $installerPackage.StartsWith($installerReleaseRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Installer input must stay under Packaged/.' }
$installerManifestPath = Join-Path $installerPackage 'release-manifest.json'
if (-not (Test-Path -LiteralPath $installerManifestPath)) { throw 'Package.ps1 must finish before creating an installer.' }
$installerManifest = Get-Content -LiteralPath $installerManifestPath -Raw | ConvertFrom-Json
foreach ($installerFile in $installerManifest.files) {
    $installerSource = [IO.Path]::GetFullPath((Join-Path $installerPackage $installerFile.path))
    if (-not $installerSource.StartsWith($installerPackage + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe manifest path.' }
    if ((Get-FileHash -LiteralPath $installerSource -Algorithm SHA256).Hash -ne $installerFile.sha256) { throw "Package changed: $($installerFile.path)" }
}
$installerTools = Join-Path $RepoRoot 'Intermediate/InstallerTools'
$installerWix = Join-Path $installerTools 'wix.exe'
if ($RestoreTools) {
    if (-not (Test-Path -LiteralPath $installerWix)) {
        & dotnet tool install wix --version 5.0.2 --tool-path $installerTools --allow-roll-forward --source https://api.nuget.org/v3/index.json
        if ($LASTEXITCODE -ne 0) { throw 'WiX tool download failed.' }
    }
    & $installerWix extension add WixToolset.Bal.wixext/5.0.2
    if ($LASTEXITCODE -ne 0) { throw 'WiX bootstrapper extension download failed.' }
    & $installerWix extension add WixToolset.Util.wixext/5.0.2
    if ($LASTEXITCODE -ne 0) { throw 'WiX registry extension download failed.' }
}
if (-not (Test-Path -LiteralPath $installerWix)) { throw 'Run Installer.ps1 with -RestoreTools to download the pinned local tool.' }
$installerOutput = Join-Path (Split-Path $installerPackage) 'Installer'
if (Test-Path -LiteralPath $installerOutput) { throw 'Installer output already exists; use a new release directory.' }
New-Item -ItemType Directory -Path $installerOutput | Out-Null
$installerVersion = $installerManifest.version
$installerIcon = Join-Path $RepoRoot 'Build/Windows/Application.ico'
$installerMsi = Join-Path $installerOutput "101st-Floor-$installerVersion.msi"
& $installerWix build (Join-Path $RepoRoot 'Resources/Installer/Product.wxs') -arch x64 -cc (Join-Path $RepoRoot 'Intermediate/InstallerCabCache') -ct 4 -d "Version=$installerVersion" -d "Icon=$installerIcon" -d "PackageDir=$installerPackage" -o $installerMsi
if ($LASTEXITCODE -ne 0) { throw 'MSI compilation failed.' }
$installerPrerequisites = Join-Path $installerPackage 'Engine/Extras/Redist/en-us/vc_redist.x64.exe'
if (-not (Test-Path -LiteralPath $installerPrerequisites)) { throw 'Unreal prerequisites are missing from the package.' }
$installerSetup = Join-Path $installerOutput "101st-Floor-$installerVersion-Setup.exe"
& $installerWix build (Join-Path $RepoRoot 'Resources/Installer/Bundle.wxs') -arch x64 -culture es-es -ext (Join-Path $RepoRoot ".wix/extensions/WixToolset.Bal.wixext/5.0.2/wixext5/WixToolset.BootstrapperApplications.wixext.dll") -ext WixToolset.Util.wixext -d "Version=$installerVersion" -d "Icon=$installerIcon" -d "Logo=$(Join-Path $RepoRoot 'Build/Windows/InstallerLogo.bmp')" -d "Prerequisites=$installerPrerequisites" -d "Msi=$installerMsi" -o $installerSetup
if ($LASTEXITCODE -ne 0) { throw 'Setup compilation failed.' }
Get-ChildItem -LiteralPath $installerOutput -File | Where-Object Extension -in '.exe', '.msi' | Get-FileHash -Algorithm SHA256 |
    Select-Object @{Name='File'; Expression={Split-Path $_.Path -Leaf}}, Hash |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $installerOutput 'sha256.json') -Encoding utf8
Write-Output $installerSetup
