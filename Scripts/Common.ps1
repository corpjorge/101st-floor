# Shared helpers for 101st Floor scripts. Dot-source it: . "$PSScriptRoot\Common.ps1"
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:RepoRoot = Split-Path -Parent $PSScriptRoot
$script:ProjectName = 'Floor101'
$script:UProjectPath = Join-Path $script:RepoRoot "$script:ProjectName.uproject"

function Get-EngineAssociation {
    (Get-Content $script:UProjectPath -Raw | ConvertFrom-Json).EngineAssociation
}

# Resolves the Unreal Engine root for this project without hard-coded paths.
# Order: $env:UE_ROOT override -> source build registered by GUID -> Epic Games Launcher install.
function Find-UnrealEngine {
    $association = Get-EngineAssociation
    $candidates = [System.Collections.Generic.List[pscustomobject]]::new()

    if ($env:UE_ROOT) {
        $candidates.Add([pscustomobject]@{ Root = $env:UE_ROOT; Source = 'env:UE_ROOT' })
    }

    if ($association -match '^\{?[0-9A-Fa-f-]{32,36}\}?$') {
        $builds = Get-ItemProperty 'HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds' -ErrorAction SilentlyContinue
        if ($builds -and $builds.PSObject.Properties[$association]) {
            $candidates.Add([pscustomobject]@{ Root = $builds.$association; Source = 'source build (registry)' })
        }
    }
    else {
        $launcherDat = Join-Path $env:ProgramData 'Epic\UnrealEngineLauncher\LauncherInstalled.dat'
        if (Test-Path $launcherDat) {
            $entry = (Get-Content $launcherDat -Raw | ConvertFrom-Json).InstallationList |
                Where-Object { $_.AppName -eq "UE_$association" } | Select-Object -First 1
            if ($entry) {
                $candidates.Add([pscustomobject]@{ Root = $entry.InstallLocation; Source = 'Epic Games Launcher' })
            }
        }
    }

    foreach ($candidate in $candidates) {
        $versionFile = Join-Path $candidate.Root 'Engine\Build\Build.version'
        if (-not (Test-Path $versionFile)) { continue }
        $v = Get-Content $versionFile -Raw | ConvertFrom-Json
        $version = "$($v.MajorVersion).$($v.MinorVersion).$($v.PatchVersion)"
        if ($association -notmatch '^\{' -and -not $version.StartsWith("$association.")) {
            throw "Engine at '$($candidate.Root)' is $version but the project requires $association.x"
        }
        $root = (Resolve-Path $candidate.Root).Path
        return [pscustomobject]@{
            Root           = $root
            Version        = $version
            Changelist     = $v.Changelist
            Source         = $candidate.Source
            IsInstalled    = Test-Path (Join-Path $root 'Engine\Build\InstalledBuild.txt')
            EditorExe      = Join-Path $root 'Engine\Binaries\Win64\UnrealEditor.exe'
            EditorCmdExe   = Join-Path $root 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
            BuildBat       = Join-Path $root 'Engine\Build\BatchFiles\Build.bat'
            RunUAT         = Join-Path $root 'Engine\Build\BatchFiles\RunUAT.bat'
        }
    }

    throw "Unreal Engine $association not found. Install it with the Epic Games Launcher or set `$env:UE_ROOT."
}

# MSVC family preferred by UE 5.8 (docs/build.md). Unit tests use the same compiler as UnrealBuildTool.
$script:MsvcToolsetVersion = '14.50'

function Get-VsWhere {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found: install Visual Studio 2026." }
    return $vswhere
}

# CMake and Ninja shipped with Visual Studio ("C++ CMake tools for Windows"). Never taken from PATH: this machine has
# an unrelated cmake first in PATH.
function Find-VsCMake {
    $vswhere = Get-VsWhere
    $component = 'Microsoft.VisualStudio.Component.VC.CMake.Project'
    $base = 'Common7\IDE\CommonExtensions\Microsoft\CMake'
    $cmake = & $vswhere -latest -products * -requires $component -find "$base\CMake\bin\cmake.exe" | Select-Object -First 1
    $ninja = & $vswhere -latest -products * -requires $component -find "$base\Ninja\ninja.exe" | Select-Object -First 1
    if (-not $cmake -or -not $ninja) {
        throw "CMake/Ninja from Visual Studio not found. Install the '$component' component."
    }
    return [pscustomobject]@{ CMake = $cmake; CTest = Join-Path (Split-Path $cmake) 'ctest.exe'; Ninja = $ninja }
}

# Runs a script block with the MSVC $MsvcToolsetVersion x64 environment (vcvars64), then restores the caller's
# environment so interactive sessions are not polluted.
function Invoke-WithMsvcEnvironment([scriptblock]$Script) {
    $vswhere = Get-VsWhere
    $component = "Microsoft.VisualStudio.Component.VC.$MsvcToolsetVersion.18.0.x86.x64"
    $vsPath = & $vswhere -products * -requires $component -property installationPath | Select-Object -First 1
    if (-not $vsPath) { throw "MSVC $MsvcToolsetVersion is not installed (component $component)." }
    $vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'
    $installerDir = Split-Path $vswhere
    $variables = & cmd.exe /d /c "set `"PATH=$installerDir;%PATH%`" && `"$vcvars`" -vcvars_ver=$MsvcToolsetVersion >nul 2>&1 && set"
    if ($LASTEXITCODE -ne 0) { throw "vcvars64.bat -vcvars_ver=$MsvcToolsetVersion failed." }

    $saved = [Environment]::GetEnvironmentVariables('Process')
    try {
        foreach ($line in $variables) {
            if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') }
        }
        & $Script
    }
    finally {
        foreach ($name in [Environment]::GetEnvironmentVariables('Process').Keys) {
            if (-not $saved.Contains($name)) { [Environment]::SetEnvironmentVariable($name, $null, 'Process') }
        }
        foreach ($name in $saved.Keys) { [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process') }
    }
}

function Write-Step([string]$Message) {
    Write-Host "==> $Message" -ForegroundColor Cyan
}
