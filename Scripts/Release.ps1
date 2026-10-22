<#
.SYNOPSIS
    Prepares a GitHub release from a checked package and, with -Publish, publishes it (ADR-0039).
.DESCRIPTION
    Input: Packaged/<version> made by Package.ps1 -Zip from a clean tree and checked with CheckPackage.ps1, plus its
    installer from Installer.ps1. Writes Packaged/<version>/Release with the versioned portable ZIP, the update manifest
    signed with the key from UpdateSigningKey.ps1, SHA256SUMS.txt, the release notes and local-release.json (the same
    release as local files, for CheckPackage.ps1 -Check update -UpdateFeed). With -Publish it creates the GitHub release
    v<version> with gh: Setup.exe, MSI, portable ZIP, manifest, signature and checksums. Nothing is uploaded otherwise.
.EXAMPLE
    .\Scripts\Release.ps1 -Package Packaged/0.3.0
    .\Scripts\Release.ps1 -Package Packaged/0.3.0 -Publish -Target main
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Package,
    [string]$Key = '',
    # The commit or branch the tag points to; by default the commit the package was built from.
    [string]$Target = '',
    [switch]$Publish
)
. "$PSScriptRoot\Common.ps1"
. "$PSScriptRoot\UpdateSigning.ps1"
$releasePackage = [IO.Path]::GetFullPath((Join-Path $RepoRoot $Package))
if (-not $releasePackage.StartsWith([IO.Path]::GetFullPath((Join-Path $RepoRoot 'Packaged')) + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'The package must be under Packaged/.'
}
$releaseManifest = Get-Content -LiteralPath (Join-Path $releasePackage 'Windows/release-manifest.json') -Raw | ConvertFrom-Json
if ($releaseManifest.configuration -ne 'Shipping' -or $releaseManifest.dirty) { throw 'Only a Shipping package built from a clean tree can be released.' }
$releaseVersion = $releaseManifest.version
$releaseSetup = Join-Path $releasePackage "Installer/101st-Floor-$releaseVersion-Setup.exe"
$releaseMsi = Join-Path $releasePackage "Installer/101st-Floor-$releaseVersion.msi"
$releasePortable = Join-Path $releasePackage '101st-Floor-portable.zip'
foreach ($releaseInput in $releaseSetup, $releaseMsi, $releasePortable) {
    if (-not (Test-Path -LiteralPath $releaseInput)) { throw "Missing $releaseInput (Package.ps1 -Zip, then Installer.ps1)." }
}
$releaseFolder = Join-Path $releasePackage 'Release'
if (-not (Test-Path -LiteralPath $releaseFolder)) {
    New-Item -ItemType Directory -Path $releaseFolder | Out-Null
    Copy-Item -LiteralPath $releasePortable -Destination (Join-Path $releaseFolder "101st-Floor-$releaseVersion-portable.zip")
    $releaseKey = if ($Key) { Import-F101UpdateKey $Key } else { Import-F101UpdateKey }
    $releaseSigned = New-F101UpdateManifest $releaseKey $releaseVersion $releaseSetup $releaseFolder
    Write-Step "Update manifest signed: $releaseSigned"
}
$releaseAssets = @($releaseSetup, $releaseMsi, (Join-Path $releaseFolder "101st-Floor-$releaseVersion-portable.zip"),
    (Join-Path $releaseFolder '101st-Floor-update.json'), (Join-Path $releaseFolder '101st-Floor-update.json.sig'))
# The signature must still verify against the key the application trusts (the manifest may have been touched by hand).
$releaseManifestBytes = [IO.File]::ReadAllBytes($releaseAssets[3])
if (-not (Test-F101UpdateSignature (Get-F101TrustedUpdateKey) $releaseManifestBytes (Get-Content -LiteralPath $releaseAssets[4] -Raw))) {
    throw 'The update manifest is not signed by the trusted key.'
}
$releaseSums = Join-Path $releaseFolder 'SHA256SUMS.txt'
$releaseAssets | ForEach-Object { '{0}  {1}' -f (Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash.ToLowerInvariant(), (Split-Path $_ -Leaf) } |
    Set-Content -LiteralPath $releaseSums -Encoding ascii
$releaseAssets += $releaseSums

# The same release as local files: CheckPackage.ps1 -Check update -UpdateFeed <this> checks it before publishing.
$releaseLocal = [ordered]@{ html_url = "file:///$($releaseFolder.Replace('\', '/'))"; assets = @($releaseAssets | ForEach-Object {
    [ordered]@{ name = Split-Path $_ -Leaf; browser_download_url = $_.Replace('\', '/') } }) }
$releaseLocal | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $releaseFolder 'local-release.json') -Encoding utf8

$releaseRepository = 'corpjorge/101st-floor'
$releaseConfig = Get-Content -LiteralPath (Join-Path $RepoRoot 'Config/DefaultGame.ini') -Raw
if ($releaseConfig -match '(?ms)^\[Floor101\.Update\].*?^Repository=([^\r\n]+)') { $releaseRepository = $Matches[1].Trim() }
$releaseSizeMb = [math]::Round((Get-Item -LiteralPath $releaseSetup).Length / 1MB)
$releaseNotes = Join-Path $releaseFolder 'notes.md'
@"
## 101st Floor $releaseVersion para Windows x64

**Instalar (recomendado):** descarga ``101st-Floor-$releaseVersion-Setup.exe`` ($releaseSizeMb MB) y ábrelo. Instala la aplicación para tu
usuario con accesos directos en el Escritorio y en Inicio; la primera vez puede pedir permiso para el runtime de
Visual C++. Las copias instaladas buscan las versiones nuevas al arrancar y se actualizan desde **Opciones → Acerca de**.

**Portable:** descarga ``101st-Floor-$releaseVersion-portable.zip``, extráelo entero y abre ``Windows\Floor101.exe``.

Windows SmartScreen puede avisar de que el instalador no es habitual ("Windows protegió tu PC"): pulsa
**Más información → Ejecutar de todas formas**. Comprueba la descarga con ``SHA256SUMS.txt``.

Requisitos: Windows 11 x64, GPU con DirectX 12 (Shader Model 6) y trazado de rayos por hardware (DXR) para Lumen y Foto.

Licencia MIT · usa Unreal® Engine (EULA de Epic Games) · [avisos de terceros](https://github.com/$releaseRepository/blob/main/THIRD-PARTY-NOTICES.md)
"@ | Set-Content -LiteralPath $releaseNotes -Encoding utf8

Write-Step "Release v$releaseVersion prepared in $releaseFolder"
$releaseAssets | ForEach-Object { Write-Output "  $(Split-Path $_ -Leaf)  $([math]::Round((Get-Item -LiteralPath $_).Length / 1MB, 1)) MB" }
if (-not $Publish) { return }

$releaseTarget = if ($Target) { $Target } else { $releaseManifest.commit }
& gh release view "v$releaseVersion" --repo $releaseRepository *> $null
if ($LASTEXITCODE -eq 0) { throw "v$releaseVersion is already published in $releaseRepository; publish a new version instead." }
& gh release create "v$releaseVersion" @releaseAssets --repo $releaseRepository --target $releaseTarget --title "101st Floor $releaseVersion" --notes-file $releaseNotes
if ($LASTEXITCODE -ne 0) { throw 'gh release create failed.' }
$releaseUploaded = (& gh release view "v$releaseVersion" --repo $releaseRepository --json assets | ConvertFrom-Json).assets
foreach ($releaseAsset in $releaseAssets) {
    $releaseName = Split-Path $releaseAsset -Leaf
    $releaseRemote = $releaseUploaded | Where-Object name -eq $releaseName
    if (-not $releaseRemote -or $releaseRemote.size -ne (Get-Item -LiteralPath $releaseAsset).Length) { throw "Asset $releaseName did not upload completely." }
}
Write-Step "Published https://github.com/$releaseRepository/releases/tag/v$releaseVersion"
