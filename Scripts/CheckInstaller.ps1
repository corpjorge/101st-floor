<#
.SYNOPSIS
    Install and uninstall the MSI into isolated workspace folders, preserving projects and existing shortcuts.
#>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Msi, [string]$Setup = '')
. "$PSScriptRoot/Common.ps1"
$installTestMsi = [IO.Path]::GetFullPath((Join-Path $RepoRoot $Msi))
$installTestRoot = [IO.Path]::GetFullPath((Join-Path $RepoRoot 'Packaged')) + [IO.Path]::DirectorySeparatorChar
if (-not $installTestMsi.StartsWith($installTestRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Installer input must stay under Packaged/.' }
$installTestSetup = ''
if ($Setup) {
    $installTestSetup = [IO.Path]::GetFullPath((Join-Path $RepoRoot $Setup))
    if (-not $installTestSetup.StartsWith($installTestRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Setup input must stay under Packaged/.' }
}
$installTestFolder = Join-Path $RepoRoot "Packaged/InstallerChecks/$(Get-Date -Format yyyyMMdd-HHmmss)"
$installTestApp = Join-Path $installTestFolder 'App'
$installTestDesktop = Join-Path $installTestFolder 'Desktop'
$installTestMenu = Join-Path $installTestFolder 'StartMenu'
New-Item -ItemType Directory -Path $installTestApp, $installTestDesktop, $installTestMenu | Out-Null
$installTestProject = Join-Path $installTestFolder 'UserProject.json'
Set-Content -LiteralPath $installTestProject -Value '{"installerTest":"preserve user projects"}' -Encoding utf8
$installTestProjectHash = (Get-FileHash -LiteralPath $installTestProject).Hash
# Defensive backup: never replace the user's existing development shortcut during a smoke install.
$installTestActualDesktop = Join-Path ([Environment]::GetFolderPath('Desktop')) '101st Floor.lnk'
$installTestActualMenu = Join-Path ([Environment]::GetFolderPath('Programs')) '101st Floor/101st Floor.lnk'
$installTestShortcutBackups = @($installTestActualDesktop, $installTestActualMenu | ForEach-Object {
    [pscustomobject]@{ Path = $_; Bytes = $(if (Test-Path -LiteralPath $_) { [IO.File]::ReadAllBytes($_) }); Existed = (Test-Path -LiteralPath $_) }
})
$installTestProperties = @("INSTALLFOLDER=`"$installTestApp`"", "F101DESKTOP=`"$installTestDesktop`"", "F101MENUROOT=`"$installTestMenu`"")
$installTestInstalled = $false
$installTestRemoveError = ''
try {
    $installTestProcess = Start-Process msiexec.exe -ArgumentList (@('/i', "`"$installTestMsi`"", '/qn', '/norestart', '/L*v', "`"$(Join-Path $installTestFolder 'install.log')`"") + $installTestProperties) -WindowStyle Hidden -PassThru -Wait
    if ($installTestProcess.ExitCode -notin 0,3010) { throw "MSI install failed: $($installTestProcess.ExitCode)." }
    $installTestInstalled = $true
    if (-not (Test-Path -LiteralPath (Join-Path $installTestApp 'Floor101.exe'))) { throw 'The installed executable is missing.' }
    foreach ($installTestShortcut in (Join-Path $installTestDesktop '101st Floor.lnk'), (Join-Path $installTestMenu '101st Floor/101st Floor.lnk')) {
        if (-not (Test-Path -LiteralPath $installTestShortcut)) { throw 'An isolated installation shortcut is missing.' }
    }
    & "$PSScriptRoot/CheckPackage.ps1" -Package ([IO.Path]::GetRelativePath($RepoRoot, $installTestApp)) -Check home
}
finally {
    if ($installTestInstalled) {
        $installTestRemove = Start-Process msiexec.exe -ArgumentList (@('/x', "`"$installTestMsi`"", '/qn', '/norestart', '/L*v', "`"$(Join-Path $installTestFolder 'uninstall.log')`"") + $installTestProperties) -WindowStyle Hidden -PassThru -Wait
        if ($installTestRemove.ExitCode -notin 0,3010) { $installTestRemoveError = "MSI uninstall failed: $($installTestRemove.ExitCode)." }
    }
    foreach ($installTestBackup in $installTestShortcutBackups) {
        if ($installTestBackup.Existed) { [IO.File]::WriteAllBytes($installTestBackup.Path, $installTestBackup.Bytes) }
    }
}
if ($installTestRemoveError) { throw $installTestRemoveError }
if (Test-Path -LiteralPath (Join-Path $installTestApp 'Floor101.exe')) { throw 'Uninstall left the installed executable.' }
if ((Get-FileHash -LiteralPath $installTestProject).Hash -ne $installTestProjectHash) { throw 'Uninstall changed a user project.' }
if ($installTestSetup) {
    $installTestLayout = Join-Path $installTestFolder 'OfflinePayloads'
    $installTestBundle = Start-Process -FilePath $installTestSetup -ArgumentList @('/layout', "`"$installTestLayout`"", '/quiet', '/norestart', '/log', "`"$(Join-Path $installTestFolder 'layout.log')`"") -WindowStyle Hidden -PassThru -Wait
    if ($installTestBundle.ExitCode -ne 0) { throw "Offline setup layout failed: $($installTestBundle.ExitCode)." }
    $installTestPayload = Get-ChildItem -LiteralPath $installTestLayout -Filter *.msi -File -Recurse | Select-Object -First 1
    $installTestRuntime = Get-ChildItem -LiteralPath $installTestLayout -Filter vc_redist.x64.exe -File -Recurse | Select-Object -First 1
    if (-not $installTestPayload -or -not $installTestRuntime -or (Get-FileHash -LiteralPath $installTestPayload.FullName).Hash -ne (Get-FileHash -LiteralPath $installTestMsi).Hash) { throw 'The offline setup payload is incomplete or changed.' }
    Write-Step 'PASS: setup contains the exact MSI and offline Visual C++ prerequisites.'
}
Write-Step 'PASS: MSI installation, isolated shortcuts, standalone startup and uninstall; user project preserved.'
Write-Output $installTestFolder
