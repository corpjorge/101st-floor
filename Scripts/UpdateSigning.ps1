# Signing of the update manifest (ADR-0039): ECDSA P-256 with SHA-256, the signature as r||s (64 bytes) in Base64, the
# public key as X||Y (64 bytes) in Base64, the form Windows CNG verifies in the application. The private key is a PEM
# file outside the repository. Dot-source it: . "$PSScriptRoot\UpdateSigning.ps1"

# Where the private key lives unless a path is given: the user's roaming profile, never the repository.
$script:DefaultUpdateKeyPath = Join-Path $env:APPDATA '101st Floor Release\update-signing-key.pem'

function Import-F101UpdateKey([string]$Path = $script:DefaultUpdateKeyPath) {
    if (-not (Test-Path -LiteralPath $Path)) { throw "No update signing key at $Path (create one with Scripts\UpdateSigningKey.ps1 -New)." }
    $key = [System.Security.Cryptography.ECDsa]::Create()
    $key.ImportFromPem((Get-Content -LiteralPath $Path -Raw))
    if ($key.KeySize -ne 256) { throw 'The update signing key must be ECDSA P-256.' }
    $key
}

function Get-F101UpdatePublicKey([System.Security.Cryptography.ECDsa]$Key) {
    $point = $Key.ExportParameters($false).Q
    [Convert]::ToBase64String([byte[]]($point.X + $point.Y))
}

function New-F101UpdateSignature([System.Security.Cryptography.ECDsa]$Key, [byte[]]$Bytes) {
    [Convert]::ToBase64String($Key.SignData($Bytes, [System.Security.Cryptography.HashAlgorithmName]::SHA256))
}

function Test-F101UpdateSignature([string]$PublicKey, [byte[]]$Bytes, [string]$Signature) {
    $point = [Convert]::FromBase64String($PublicKey)
    $parameters = [System.Security.Cryptography.ECParameters]::new()
    $parameters.Curve = [System.Security.Cryptography.ECCurve+NamedCurves]::nistP256
    $parameters.Q = [System.Security.Cryptography.ECPoint]@{ X = $point[0..31]; Y = $point[32..63] }
    $verifier = [System.Security.Cryptography.ECDsa]::Create($parameters)
    $verifier.VerifyData($Bytes, [Convert]::FromBase64String($Signature), [System.Security.Cryptography.HashAlgorithmName]::SHA256)
}

# The public key the application trusts, read from its source so a release can never be signed with another key.
function Get-F101TrustedUpdateKey {
    $source = Get-Content -LiteralPath (Join-Path $RepoRoot 'Source/Floor101/Private/Floor101Update.cpp') -Raw
    if ($source -notmatch 'UpdatePublicKey\s*=\s*TEXT\("([A-Za-z0-9+/=]{88})"\)') { throw 'The trusted update key is missing from Floor101Update.cpp.' }
    $Matches[1]
}

# Writes <Folder>/101st-Floor-update.json and its .sig for a Setup.exe; returns the manifest path.
function New-F101UpdateManifest([System.Security.Cryptography.ECDsa]$Key, [string]$Version, [string]$Setup, [string]$Folder) {
    $trusted = Get-F101TrustedUpdateKey
    if ((Get-F101UpdatePublicKey $Key) -ne $trusted) { throw 'This key is not the one the application trusts (Floor101Update.cpp).' }
    $manifest = [ordered]@{
        product = '101st Floor'
        version = $Version
        setup = [ordered]@{
            name = Split-Path $Setup -Leaf
            size = (Get-Item -LiteralPath $Setup).Length
            sha256 = (Get-FileHash -LiteralPath $Setup -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    }
    $path = Join-Path $Folder '101st-Floor-update.json'
    $bytes = [Text.UTF8Encoding]::new($false).GetBytes(($manifest | ConvertTo-Json -Depth 4))
    [IO.File]::WriteAllBytes($path, $bytes)
    $signature = New-F101UpdateSignature $Key $bytes
    if (-not (Test-F101UpdateSignature $trusted $bytes $signature)) { throw 'The update signature does not verify.' }
    Set-Content -LiteralPath "$path.sig" -Value $signature -Encoding ascii -NoNewline
    $path
}
