<#
.SYNOPSIS
    Checks that this machine can build and run 101st Floor. Installs nothing.
.DESCRIPTION
    Automates the Phase A environment checks (docs/build.md). FAIL = required and missing; WARN = works but differs
    from the validated setup; INFO = optional tool. Exit code 0 when there is no FAIL.
.EXAMPLE
    .\Scripts\Setup.ps1
#>
[CmdletBinding()]
param()

. "$PSScriptRoot\Common.ps1"

$results = [System.Collections.Generic.List[pscustomobject]]::new()
function Add-Result([string]$Check, [string]$Status, [string]$Detail) {
    $results.Add([pscustomobject]@{ Check = $Check; Status = $Status; Detail = $Detail })
}
function Test-Step([string]$Check, [scriptblock]$Probe) {
    try { & $Probe } catch { Add-Result $Check 'FAIL' $_.Exception.Message }
}

Test-Step 'Windows 11 x64' {
    $os = Get-CimInstance Win32_OperatingSystem
    $ok = [int]$os.BuildNumber -ge 22000 -and $env:PROCESSOR_ARCHITECTURE -eq 'AMD64'
    Add-Result 'Windows 11 x64' ($(if ($ok) { 'PASS' } else { 'FAIL' })) "$($os.Caption) build $($os.BuildNumber) $env:PROCESSOR_ARCHITECTURE"
}

Test-Step 'PowerShell 7' {
    $ok = $PSVersionTable.PSVersion.Major -ge 7
    Add-Result 'PowerShell 7' ($(if ($ok) { 'PASS' } else { 'FAIL' })) "$($PSVersionTable.PSVersion)"
}

Test-Step 'Git + LFS' {
    $git = (git --version) 2>$null
    $lfs = (git lfs version) 2>$null
    $longPaths = (git config --get core.longpaths) 2>$null
    $status = if (-not $git -or -not $lfs) { 'FAIL' } elseif ($longPaths -ne 'true') { 'WARN' } else { 'PASS' }
    Add-Result 'Git + LFS' $status "$git; $(($lfs -split ' ')[0]); core.longpaths=$longPaths"
}

Test-Step 'Git Bash' {
    $bash = Join-Path $env:ProgramFiles 'Git\bin\bash.exe'
    Add-Result 'Git Bash (Claude Code hooks)' ($(if (Test-Path $bash) { 'PASS' } else { 'WARN' })) $bash
}

Test-Step 'MSVC' {
    $vswhere = Get-VsWhere
    $component = "Microsoft.VisualStudio.Component.VC.$MsvcToolsetVersion.18.0.x86.x64"
    $vs = & $vswhere -products * -requires $component -format json | ConvertFrom-Json | Select-Object -First 1
    if ($vs) { Add-Result "MSVC $MsvcToolsetVersion (UE 5.8 preferred)" 'PASS' "$($vs.displayName) $($vs.catalog.productDisplayVersion)" }
    else { Add-Result "MSVC $MsvcToolsetVersion (UE 5.8 preferred)" 'FAIL' "missing component $component" }
}

Test-Step 'CMake' {
    $tools = Find-VsCMake
    Add-Result 'CMake + Ninja (Visual Studio)' 'PASS' ((& $tools.CMake --version | Select-Object -First 1))
}

Test-Step 'Windows SDK' {
    $sdks = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\Include" -Directory -ErrorAction Stop |
        Where-Object { $_.Name -match '^10\.0\.\d+\.\d+$' -and [version]$_.Name -ge [version]'10.0.22621.0' }
    $status = if ($sdks) { 'PASS' } else { 'FAIL' }
    Add-Result 'Windows SDK >= 10.0.22621' $status (($sdks | ForEach-Object Name) -join ', ')
}

Test-Step 'Unreal Engine' {
    $engine = Find-UnrealEngine
    Add-Result "Unreal Engine $(Get-EngineAssociation)" 'PASS' "$($engine.Version) (CL $($engine.Changelist), $($engine.Source)) $($engine.Root)"
}

Test-Step 'GPU' {
    $gpu = Get-CimInstance Win32_VideoController | Where-Object { $_.Name -match 'NVIDIA|AMD|Radeon|Intel\(R\) Arc' } | Select-Object -First 1
    if (-not $gpu) { Add-Result 'GPU' 'FAIL' 'no discrete GPU found'; return }
    $status = if ($gpu.Name -match 'RTX 5070 Ti') { 'PASS' } elseif ($gpu.Name -match 'RTX') { 'WARN' } else { 'FAIL' }
    Add-Result 'GPU (reference: RTX 5070 Ti)' $status "$($gpu.Name), driver $($gpu.DriverVersion); DX12/SM6/HWRT: run CheckRendering.ps1"
}

Test-Step 'Claude Code' {
    $claude = Get-Command claude -ErrorAction SilentlyContinue
    if (-not $claude) { Add-Result 'Claude Code' 'INFO' 'not installed (optional for building)'; return }
    Add-Result 'Claude Code' 'PASS' ((& claude --version) 2>$null)
    $settings = Get-Content (Join-Path $RepoRoot '.claude\settings.json') -Raw | ConvertFrom-Json
    $plugin = 'unreal-engine-skills-for-claude-code@claude-plugins-official'
    $enabled = $settings.enabledPlugins.PSObject.Properties[$plugin] -and $settings.enabledPlugins.$plugin
    Add-Result 'Epic Unreal plugin (project scope)' ($(if ($enabled) { 'PASS' } else { 'WARN' })) $plugin
}

Test-Step 'Optional tools' {
    $blender = Get-ChildItem "$env:ProgramFiles\Blender Foundation" -Filter blender.exe -Recurse -Depth 2 -ErrorAction SilentlyContinue | Select-Object -First 1
    Add-Result 'Blender' 'INFO' ($(if ($blender) { "$($blender.VersionInfo.ProductVersion) $($blender.FullName)" } else { 'not found' }))
    $launcherDat = Join-Path $env:ProgramData 'Epic\UnrealEngineLauncher\LauncherInstalled.dat'
    $installed = if (Test-Path $launcherDat) { (Get-Content $launcherDat -Raw | ConvertFrom-Json).InstallationList } else { @() }
    foreach ($product in 'Twinmotion', 'RealityScan') {
        $hits = @($installed | Where-Object { $_.InstallLocation -match $product } | ForEach-Object { Split-Path $_.InstallLocation -Leaf })
        Add-Result $product 'INFO' ($(if ($hits) { $hits -join ', ' } else { 'not found' }))
    }
}

$results | Format-Table -AutoSize -Wrap | Out-Host
$failed = @($results | Where-Object Status -eq 'FAIL')
if ($failed.Count -gt 0) {
    Write-Host "$($failed.Count) required check(s) failed. See docs/build.md." -ForegroundColor Red
    exit 1
}
Write-Step 'Environment ready. Next: .\Scripts\Build.ps1 and .\Scripts\Test.ps1'
exit 0
