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

function Write-Step([string]$Message) {
    Write-Host "==> $Message" -ForegroundColor Cyan
}
