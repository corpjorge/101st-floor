<#
.SYNOPSIS
    Runs the 101st Floor tests.
.DESCRIPTION
    1. Architecture: pure modules (CadCore, Geometry, SceneModel, Drawing, Assets, Exporters) must not include or use Unreal (ADR-0002).
    2. Unit tests: pure modules built with CMake + Ninja + MSVC 14.50, doctest, no Unreal (seconds).
    3. With -Integration: Unreal Automation tests "Floor101.*" in a headless editor (-nullrhi). Needs a current
       Build.ps1 and the editor closed or at least not holding the tests' world.
    Exit code 0 = everything run passed.
.EXAMPLE
    .\Scripts\Test.ps1
    .\Scripts\Test.ps1 -Integration
#>
[CmdletBinding()]
param(
    [switch]$Integration,
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug'
)

. "$PSScriptRoot\Common.ps1"

$PureModules = 'Floor101CadCore', 'Floor101Geometry', 'Floor101SceneModel', 'Floor101Drawing', 'Floor101Assets', 'Floor101Exporters'
$failures = [System.Collections.Generic.List[string]]::new()

function Test-Architecture {
    Write-Step 'Architecture: pure modules must not depend on Unreal'
    $violations = [System.Collections.Generic.List[string]]::new()
    $unrealInclude = '^\s*#\s*include\s+[<"](CoreMinimal\.h|Engine/|UObject/|Modules/|HAL/|Misc/|Containers/|Math/|Templates/|Components/|GameFramework/|.*\.generated\.h)'
    $unrealType = '\b(FVector\w*|FTransform|FString|FName|FText|TArray|TMap|TSharedPtr|UObject|AActor|USceneComponent|UWorld)\b'
    foreach ($module in $PureModules) {
        $dir = Join-Path $RepoRoot "Source\$module"
        foreach ($file in Get-ChildItem $dir -Recurse -Include *.h, *.cpp | Where-Object { $_.Name -notlike '*Module.cpp' }) {
            foreach ($hit in Select-String -Path $file.FullName -Pattern $unrealInclude, $unrealType) {
                $violations.Add("$($file.FullName.Replace("$RepoRoot\", '')):$($hit.LineNumber): $($hit.Line.Trim())")
            }
        }
        # Build.cs: only Core (module registration) and other pure modules are allowed.
        $dependencyLines = Get-Content (Join-Path $dir "$module.Build.cs") | Where-Object { $_ -match 'DependencyModuleNames' }
        $deps = [regex]::Matches(($dependencyLines -join "`n"), '"(\w+)"') | ForEach-Object { $_.Groups[1].Value }
        foreach ($dep in $deps | Where-Object { $_ -ne 'Core' -and $_ -notin $PureModules }) {
            $violations.Add("$module.Build.cs depends on '$dep'")
        }
    }
    if ($violations.Count -gt 0) {
        $violations | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
        $failures.Add("architecture ($($violations.Count) violation(s))")
    }
    else {
        Write-Step "Architecture OK ($($PureModules -join ', '))"
    }
}

function Invoke-UnitTests {
    $tools = Find-VsCMake
    $buildDir = Join-Path $RepoRoot "Intermediate\UnitTests\$Configuration"
    Write-Step "Unit tests: CMake $((& $tools.CMake --version | Select-Object -First 1) -replace 'cmake version ', '') + Ninja + MSVC $MsvcToolsetVersion ($Configuration)"
    $exitCode = Invoke-WithMsvcEnvironment {
        & $tools.CMake -S (Join-Path $RepoRoot 'Tests\Unit') -B $buildDir -G Ninja "-DCMAKE_MAKE_PROGRAM=$($tools.Ninja)" `
            "-DCMAKE_BUILD_TYPE=$Configuration" '-DCMAKE_CXX_COMPILER=cl.exe' --log-level=WARNING | Out-Host
        if ($LASTEXITCODE -ne 0) { return $LASTEXITCODE }
        & $tools.CMake --build $buildDir | Out-Host
        if ($LASTEXITCODE -ne 0) { return $LASTEXITCODE }
        & (Join-Path $buildDir 'Floor101UnitTests.exe') | Out-Host
        return $LASTEXITCODE
    }
    if ($exitCode -ne 0) { $failures.Add("unit tests (exit $exitCode)") }
}

function Invoke-IntegrationTests {
    $engine = Find-UnrealEngine
    $reportDir = Join-Path $RepoRoot ("Saved\Automation\{0:yyyyMMdd-HHmmss}" -f (Get-Date))
    $logPath = Join-Path $reportDir 'Automation.log'
    New-Item -ItemType Directory -Force $reportDir | Out-Null
    Write-Step "Integration: Unreal Automation 'Floor101.' (headless editor, -nullrhi)"
    $arguments = @(
        "`"$UProjectPath`"", '-ExecCmds="Automation RunTests Floor101."', '-TestExit="Automation Test Queue Empty"',
        "-ReportExportPath=`"$reportDir`"", '-unattended', '-nullrhi', '-nosplash', '-nosound', '-nopause',
        '-log', "-abslog=`"$logPath`""
    )
    $process = Start-Process -FilePath $engine.EditorCmdExe -ArgumentList $arguments -PassThru -NoNewWindow -Wait
    $report = Join-Path $reportDir 'index.json'
    if (-not (Test-Path $report)) {
        $failures.Add("integration tests: no report (editor exit $($process.ExitCode), log $logPath)")
        return
    }
    $json = Get-Content $report -Raw | ConvertFrom-Json
    foreach ($test in $json.tests) {
        $color = if ($test.state -eq 'Success') { 'Green' } else { 'Red' }
        Write-Host ("  [{0}] {1} ({2:N2}s)" -f $test.state, $test.fullTestPath, $test.duration) -ForegroundColor $color
        foreach ($entry in $test.entries | Where-Object { $_.event.type -in 'Error', 'Warning' }) {
            Write-Host "      $($entry.event.type): $($entry.event.message)" -ForegroundColor Yellow
        }
    }
    Write-Step "Integration: $($json.succeeded) passed, $($json.failed) failed, $($json.notRun) not run (report $reportDir)"
    if ($json.tests.Count -eq 0) { $failures.Add('integration tests: none found') }
    elseif ($json.failed -gt 0 -or $json.succeeded -eq 0) { $failures.Add("integration tests ($($json.failed) failed)") }
}

Test-Architecture
Invoke-UnitTests
if ($Integration) { Invoke-IntegrationTests }

if ($failures.Count -gt 0) {
    Write-Host "FAILED: $($failures -join '; ')" -ForegroundColor Red
    exit 1
}
Write-Step 'All tests passed'
exit 0
