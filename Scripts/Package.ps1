<#
.SYNOPSIS
    Build, cook and archive a standalone Windows application. Outputs stay in Packaged/.
#>
[CmdletBinding()]
param(
    [ValidateSet('Development', 'Shipping')][string]$Configuration = 'Shipping',
    [string]$Output = '',
    [switch]$SkipBuild,
    [switch]$Zip
)
. "$PSScriptRoot/Common.ps1"
$packageEngine = Find-UnrealEngine
$packageVersionSetting = Select-String -LiteralPath (Join-Path $RepoRoot 'Config/DefaultGame.ini') -Pattern '^ProjectVersion=(\d+\.\d+\.\d+)$' | Select-Object -First 1
if (-not $packageVersionSetting) { throw 'Config/DefaultGame.ini must define ProjectVersion as major.minor.patch.' }
$packageVersion = $packageVersionSetting.Matches[0].Groups[1].Value
if (-not $Output) { $Output = "Packaged/$packageVersion-$(Get-Date -Format yyyyMMdd-HHmmss)-$Configuration" }
$packageOutput = [IO.Path]::GetFullPath((Join-Path $RepoRoot $Output))
$packageRoot = [IO.Path]::GetFullPath((Join-Path $RepoRoot 'Packaged')) + [IO.Path]::DirectorySeparatorChar
if (-not $packageOutput.StartsWith($packageRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Package output must be a subdirectory of Packaged/.'
}
if (Test-Path -LiteralPath $packageOutput) { throw 'Choose a new output directory; existing releases are never overwritten.' }
if (Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'" | Where-Object { $_.CommandLine -like "*$ProjectName.uproject*" }) {
    throw 'Close the editor and development game before packaging.'
}
New-Item -ItemType Directory -Path $packageOutput | Out-Null
$packageArguments = @('BuildCookRun', '-nop4', "-project=$UProjectPath", '-platform=Win64',
    "-clientconfig=$Configuration", '-cook', '-stage', '-pak', '-iostore', '-archive', '-prereqs',
    '-nodebuginfo', '-unattended', '-utf8output', "-archivedirectory=$packageOutput",
    '-UbtArgs=-UBADisableRemote -UBAHost=127.0.0.1')
if (-not $SkipBuild) { $packageArguments += '-build' }
Write-Step "Packaging $ProjectName $Configuration; log: $packageOutput/packaging.log"
& $packageEngine.RunUAT @packageArguments 2>&1 | Tee-Object -FilePath (Join-Path $packageOutput 'packaging.log') | Out-Host
if ($LASTEXITCODE -ne 0) { throw "Packaging failed. Preserve $packageOutput/packaging.log for diagnosis." }
$packageWindows = Join-Path $packageOutput 'Windows'
if (-not (Test-Path -LiteralPath (Join-Path $packageWindows 'Floor101.exe'))) { throw 'The archived application executable is missing.' }
$packageLibrary = Join-Path $packageWindows 'Floor101/Library'
New-Item -ItemType Directory -Force -Path $packageLibrary | Out-Null
foreach ($packageCategory in 'furniture', 'textures') {
    Copy-Item -LiteralPath (Join-Path $RepoRoot "Library/$packageCategory") -Destination $packageLibrary -Recurse
}
(Get-Content -LiteralPath (Join-Path $RepoRoot 'Resources/Distribution/LEEME.txt') -Raw).Replace('{VERSION}', $packageVersion) |
    Set-Content -LiteralPath (Join-Path $packageWindows 'LEEME.txt') -Encoding utf8 -NoNewline
# The project's license and the third-party notices (ADR-0039), next to Floor101.exe: Options > Acerca de opens them.
Copy-Item -LiteralPath (Join-Path $RepoRoot 'LICENSE') -Destination (Join-Path $packageWindows 'LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $RepoRoot 'THIRD-PARTY-NOTICES.md') -Destination (Join-Path $packageWindows 'THIRD-PARTY-NOTICES.txt')
$packageLicenses = Join-Path $packageWindows 'Licenses'
New-Item -ItemType Directory -Path $packageLicenses | Out-Null
Copy-Item -LiteralPath (Join-Path $RepoRoot 'ThirdParty/nlohmann/LICENSE.MIT') -Destination (Join-Path $packageLicenses 'nlohmann-json-MIT.txt')
Copy-Item -LiteralPath (Join-Path $RepoRoot 'Library/README.md') -Destination (Join-Path $packageLicenses 'Library.md')
$packageFiles = @(Get-ChildItem -LiteralPath $packageWindows -File -Recurse | ForEach-Object {
    [pscustomobject]@{ path = [IO.Path]::GetRelativePath($packageWindows, $_.FullName).Replace('\', '/');
        bytes = $_.Length; sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
})
[pscustomobject]@{ product = '101st Floor'; version = $packageVersion; configuration = $Configuration;
    commit = (& git -C $RepoRoot rev-parse HEAD).Trim(); dirty = [bool](@(& git -C $RepoRoot status --porcelain).Count);
    engine = $packageEngine.Version; files = $packageFiles } |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $packageWindows 'release-manifest.json') -Encoding utf8
if ($Zip) { Compress-Archive -LiteralPath $packageWindows -DestinationPath (Join-Path $packageOutput '101st-Floor-portable.zip') }
Write-Output $packageWindows
