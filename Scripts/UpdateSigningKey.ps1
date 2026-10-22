<#
.SYNOPSIS
    Creates or shows the key that signs 101st Floor updates (ADR-0039).
.DESCRIPTION
    -New writes a new ECDSA P-256 private key as PEM outside the repository (by default
    %APPDATA%\101st Floor Release\update-signing-key.pem) and prints its public key, which goes into
    Source/Floor101/Private/Floor101Update.cpp (UpdatePublicKey). Keep a backup of the private key: without it no
    installed copy will accept a new version. Without -New it prints the public key of the existing private key.
.EXAMPLE
    .\Scripts\UpdateSigningKey.ps1 -New
    .\Scripts\UpdateSigningKey.ps1
#>
[CmdletBinding()]
param([switch]$New, [string]$Path = '')
. "$PSScriptRoot\Common.ps1"
. "$PSScriptRoot\UpdateSigning.ps1"
if (-not $Path) { $Path = $DefaultUpdateKeyPath }
$keyPath = [IO.Path]::GetFullPath($Path)
if ($keyPath.StartsWith([IO.Path]::GetFullPath($RepoRoot) + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'The private key must stay outside the repository.'
}
if ($New) {
    if (Test-Path -LiteralPath $keyPath) { throw "A key already exists at $keyPath; replacing it would orphan every installed copy." }
    New-Item -ItemType Directory -Force -Path (Split-Path $keyPath) | Out-Null
    $key = [System.Security.Cryptography.ECDsa]::Create([System.Security.Cryptography.ECCurve+NamedCurves]::nistP256)
    Set-Content -LiteralPath $keyPath -Value $key.ExportPkcs8PrivateKeyPem() -Encoding ascii
    Write-Step "Private key written to $keyPath. Back it up somewhere safe; never commit it."
}
$publicKey = Get-F101UpdatePublicKey (Import-F101UpdateKey $keyPath)
Write-Step 'Public key (UpdatePublicKey in Source/Floor101/Private/Floor101Update.cpp):'
Write-Output $publicKey
