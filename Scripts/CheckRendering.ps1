<#
.SYNOPSIS
    Validates the real rendering path (DX12, SM6, GPU, hardware ray tracing, Lumen HWRT, VSM, Nanite)
    by running the project and parsing engine logs. Nothing is assumed from configuration alone.
.DESCRIPTION
    Launches Floor101 in -game mode with -F101RenderCheck (Source/Floor101/Private/RenderCheck.cpp),
    which waits for shaders, samples frame/GPU time, logs the runtime state, runs ProfileGPU, path traces the view
    (photo mode, ADR-0018) up to its samples, saves that image and exits.
    The first run compiles thousands of shaders and can take a long time; later runs use the DDC.
    Exit code 0 = every check passed.
.EXAMPLE
    .\Scripts\CheckRendering.ps1
#>
[CmdletBinding()]
param(
    [string]$Map = '/Engine/Maps/Templates/Template_Default',
    [string]$ExpectedGpu = 'RTX 5070 Ti',
    [int]$ResX = 1920,
    [int]$ResY = 1080,
    [int]$WarmupFrames = 120,
    [int]$SampleFrames = 300,
    [int]$TimeoutSeconds = 3600,
    # Design mode target is 30-60+ FPS; above this average GPU time the check warns.
    [double]$MaxAvgGpuMs = 33.3,
    # Samples per pixel the path tracer must reach (photo mode, ADR-0018); 0 skips that check.
    [int]$PathTraceSamples = 64,
    # Re-analyze an existing render check log without launching the engine.
    [string]$AnalyzeLog,
    # Load the MVP 0 reference wall (-F101Demo) so generated geometry is part of the validated frame.
    [switch]$Demo
)

. "$PSScriptRoot\Common.ps1"

function Invoke-RenderCheckRun {
    $engine = Find-UnrealEngine
    $logDir = Join-Path $RepoRoot 'Saved\RenderCheck'
    New-Item -ItemType Directory -Force $logDir | Out-Null
    $runLog = Join-Path $logDir ("RenderCheck-{0:yyyyMMdd-HHmmss}.log" -f (Get-Date))

    $arguments = @(
        "`"$UProjectPath`"", $Map, '-game', '-windowed', "-ResX=$ResX", "-ResY=$ResY",
        '-log', "-abslog=`"$runLog`"", '-unattended', '-nosplash', '-nosound', '-DisablePython', # as Run.ps1
        '-ExecCmds="r.ProfileGPU.ShowUI 0, r.ProfileGPU.UnicodeOutput 0"',
        '-F101RenderCheck', "-F101RenderCheckWarmupFrames=$WarmupFrames",
        "-F101RenderCheckSampleFrames=$SampleFrames", "-F101RenderCheckTimeout=$TimeoutSeconds",
        "-F101RenderCheckPathTraceSamples=$PathTraceSamples"
    )
    if ($Demo) { $arguments += '-F101Demo' }

    Write-Step "Unreal Engine $($engine.Version): $($engine.EditorExe)"
    Write-Step "Running render check on $Map (log: $runLog)"
    $started = Get-Date
    $process = Start-Process -FilePath $engine.EditorExe -ArgumentList $arguments -PassThru
    # The in-process check has its own timeout; this is a safety net if the process hangs.
    if (-not $process.WaitForExit(($TimeoutSeconds + 300) * 1000)) {
        Stop-Process -Id $process.Id -Force
        throw "Render check process hung and was killed. Log: $runLog"
    }
    Write-Step ("Process exited with code {0} after {1:N0}s" -f $process.ExitCode, ((Get-Date) - $started).TotalSeconds)
    return $runLog
}

if ($AnalyzeLog) {
    $logPath = (Resolve-Path $AnalyzeLog).Path
    Write-Step "Analyzing existing log: $logPath"
}
else {
    $logPath = Invoke-RenderCheckRun
}

if (-not (Test-Path $logPath)) { throw "Log not found: $logPath" }
$log = Get-Content $logPath

# Runtime state reported by RenderCheck.cpp as "F101RenderCheck: key=value".
$state = @{}
foreach ($line in $log) {
    if ($line -match 'F101RenderCheck: ([\w.]+)=(.*)$') { $state[$Matches[1]] = $Matches[2].Trim() }
}

$results = [System.Collections.Generic.List[pscustomobject]]::new()
function Add-Check([string]$Name, [string]$Status, [string]$Detail) {
    $results.Add([pscustomobject]@{ Check = $Name; Status = $Status; Detail = $Detail })
}
function Test-LogLine([string]$Pattern) { [bool]($log | Select-String -Pattern $Pattern -SimpleMatch -Quiet) }
function Get-LogLine([string]$Pattern) {
    $match = $log | Select-String -Pattern $Pattern | Select-Object -First 1
    if ($match) { $match.Line } else { $null }
}
function Pass-If([string]$Name, [bool]$Condition, [string]$Detail) {
    Add-Check $Name ($(if ($Condition) { 'PASS' } else { 'FAIL' })) $Detail
}

Pass-If 'Check completed' ($state['result'] -eq 'complete') "result=$($state['result'])"

Pass-If 'DX12 RHI active' ((Test-LogLine 'RHI D3D12 with Feature Level SM6 is supported and will be used') -and $state['rhi'] -eq 'D3D12') `
    "rhi=$($state['rhi']) feature_level=$($state['feature_level'])"

$fallback = Get-LogLine 'attempting to fall back|Loading RHI module D3D11RHI'
Pass-If 'No DX11 / RHI fallback' (-not $fallback) ($(if ($fallback) { $fallback.Trim() } else { 'none' }))

Pass-If 'SM6 shader platform' ($state['shader_platform'] -eq 'PCD3D_SM6') "shader_platform=$($state['shader_platform'])"

$chosen = Get-LogLine 'Chosen D3D12 Adapter Id'
Pass-If "GPU is $ExpectedGpu" ($state['adapter'] -like "*$ExpectedGpu*") "adapter=$($state['adapter']) driver=$($state['driver']) ($($chosen -replace '^.*LogD3D12RHI: ',''))"

Pass-If 'SM6.6 atomic64' ((Test-LogLine 'Shader Model 6.6 atomic64 is supported') -and $state['rhi_atomic64'] -eq '1') "rhi_atomic64=$($state['rhi_atomic64'])"

Pass-If 'Hardware ray tracing (RHI)' ((Test-LogLine 'D3D12 ray tracing tier 1.1 and bindless resources are supported') -and $state['rhi_ray_tracing'] -eq '1') `
    "shaders=$($state['rhi_ray_tracing_shaders']) inline=$($state['rhi_inline_ray_tracing'])"

$rtLine = Get-LogLine 'LogRendererCore: Ray tracing is'
Pass-If 'Ray tracing enabled (renderer)' ($state['ray_tracing_enabled'] -eq '1') (($rtLine -replace '^.*LogRendererCore: ', '').Trim())

Pass-If 'Lumen GI + reflections' (($state['cvar.r.DynamicGlobalIlluminationMethod'] -eq '1') -and ($state['cvar.r.ReflectionMethod'] -eq '1') -and ($state['lumen_gi_supported'] -eq '1')) `
    "GI=$($state['cvar.r.DynamicGlobalIlluminationMethod']) Reflections=$($state['cvar.r.ReflectionMethod'])"

# ProfileGPU is the runtime proof: Lumen HWRT passes must appear in the captured frame.
# It prints one table per GPU queue ("<Queue> pipeline N - GPU N"); Lumen GI runs on async compute by default,
# and that table is printed before the graphics one, so every queue section is scanned.
$profileStart = [Array]::FindIndex([string[]]$log, [Predicate[string]] { param($l) $l -match 'pipeline \d+ - GPU \d+' })
$hwrtPasses = @()
if ($profileStart -ge 0) {
    $queue = ''
    $hwrtPasses = @(foreach ($line in $log[$profileStart..($log.Count - 1)]) {
        if ($line -match '(\w+) pipeline \d+ - GPU \d+') { $queue = $Matches[1]; continue }
        $columns = $line -split '\|'
        # Table rows: 12 numeric columns followed by the (indented) event name.
        if ($columns.Count -ge 14 -and $columns[13] -match 'HardwareRayTracing|HWRT') {
            [pscustomobject]@{ Queue = $queue; Time = $columns[12].Trim(); Pass = $columns[13].Trim() }
        }
    })
}
$hwrtQueues = ($hwrtPasses | Group-Object Queue | ForEach-Object { "$($_.Name)=$($_.Count)" }) -join ' '
Pass-If 'Lumen hardware ray tracing (runtime)' (($state['lumen_hwrt_expected'] -eq '1') -and $hwrtPasses.Count -gt 0) `
    "expected=$($state['lumen_hwrt_expected']) HWRT passes in ProfileGPU: $($hwrtPasses.Count) ($hwrtQueues)"

Pass-If 'Virtual Shadow Maps' ($state['virtual_shadow_maps'] -eq '1') "virtual_shadow_maps=$($state['virtual_shadow_maps'])"
Pass-If 'Nanite' ($state['nanite'] -eq '1') "nanite=$($state['nanite'])"

$critical = @($log | Select-String -Pattern '(LogD3D12RHI|LogRHI|LogShaderCompilers|LogRenderer|LogRendererCore|LogMaterial)\w*: (Error|Fatal)' |
    ForEach-Object { $_.Line.Trim() })
Pass-If 'No critical RHI/shader errors' ($critical.Count -eq 0) "$($critical.Count) error line(s)"

# Lumen surface cache coverage (r.LumenScene.DumpStats 1): generated walls must bring their own mesh cards (ADR-0005).
$statsStart = [Array]::FindIndex([string[]]$log, [Predicate[string]] { param($l) $l -match '\*\*\* LumenScene Stats \*\*\*' })
$lumenCards = 'n/a'
if ($statsStart -ge 0) {
    $block = $log[$statsStart..[Math]::Min($log.Count - 1, $statsStart + 8)] -join "`n"
    $meshCards = if ($block -cmatch 'Mesh cards: (\d+)') { $Matches[1] } else { '?' }
    $cards = if ($block -cmatch ' Cards: (\d+)') { $Matches[1] } else { '?' }
    $lumenCards = "mesh cards=$meshCards cards=$cards"
}
Add-Check 'Lumen scene cards' ($(if ($statsStart -ge 0) { 'INFO' } else { 'WARN' })) $lumenCards

$wallLine = Get-LogLine 'F101: wall .* -> actor '
if ($Demo -or $wallLine) {
    Pass-If 'Reference wall rendered' ([bool]$wallLine) ($(if ($wallLine) { ($wallLine -replace '^.*F101: ', '').Trim() } else { 'no F101 wall in the log' }))
    # The finish material (ADR-0012) must load: without it walls would fall back to the engine default material.
    $missingFinish = Get-LogLine 'F101: finish material .* is missing|Failed to find /Game/Floor101/'
    Pass-If 'Finish material loaded' (-not $missingFinish) ($(if ($missingFinish) { ($missingFinish -replace '^.*(F101: |Failed)', '$1').Trim() } else { '/Game/Floor101/Materials/M_Floor101Finish, M_Floor101Glass' }))
}

# Photo mode (ADR-0018): the path tracer reaches its samples and the saved image is not black.
if ($state.ContainsKey('path_tracing_target')) {
    $target = [int]$state['path_tracing_target']
    $samples = if ($state['path_tracing_samples']) { [int]$state['path_tracing_samples'] } else { 0 }
    $image = $state['path_tracing_image']
    $brightness = [double]::NaN
    if ($image -and (Test-Path $image)) {
        Add-Type -AssemblyName System.Drawing
        $bitmap = [System.Drawing.Bitmap]::FromFile($image)
        try {
            $sum = 0.0; $count = 0
            for ($y = 0; $y -lt $bitmap.Height; $y += [Math]::Max(1, [int]($bitmap.Height / 40))) {
                for ($x = 0; $x -lt $bitmap.Width; $x += [Math]::Max(1, [int]($bitmap.Width / 40))) {
                    $sum += $bitmap.GetPixel($x, $y).GetBrightness(); $count++
                }
            }
            $brightness = $sum / [Math]::Max(1, $count)
        }
        finally { $bitmap.Dispose() }
    }
    Pass-If 'Path tracing (photo mode)' (($samples -ge $target) -and ($brightness -gt 0.05)) `
        ("samples={0}/{1} in {2}; image brightness={3:N2} {4}" -f $samples, $target, $state['path_tracing_time'], $brightness, $image)
}
elseif ($PathTraceSamples -gt 0) {
    Add-Check 'Path tracing (photo mode)' 'FAIL' 'the render check did not reach the path tracing phase'
}

$gpuAvg = if ($state['gpu_time'] -match 'avg=([\d.]+)ms') { [double]$Matches[1] } else { [double]::NaN }
$perfStatus = if ([double]::IsNaN($gpuAvg)) { 'FAIL' } elseif ($gpuAvg -le $MaxAvgGpuMs) { 'PASS' } else { 'WARN' }
Add-Check 'Frame time' $perfStatus "gpu $($state['gpu_time']) | frame $($state['frame_time']) @ $($state['resolution'])"

$results | Format-Table -AutoSize -Wrap | Out-Host
if ($critical.Count -gt 0) {
    Write-Host 'Critical lines:' -ForegroundColor Yellow
    $critical | Select-Object -First 20 | ForEach-Object { Write-Host "  $_" }
}
if ($hwrtPasses.Count -gt 0) {
    Write-Host 'Hardware ray tracing passes captured by ProfileGPU:'
    $hwrtPasses | Format-Table Queue, Time, Pass -AutoSize | Out-Host
}
Write-Host "Shaders ready after: $($state['shaders_ready_after'])  |  Log: $logPath"

$failed = @($results | Where-Object Status -eq 'FAIL')
if ($failed.Count -gt 0) {
    Write-Host "$($failed.Count) check(s) FAILED" -ForegroundColor Red
    exit 1
}
Write-Step 'All rendering checks passed'
exit 0
