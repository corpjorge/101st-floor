<#
.SYNOPSIS
    Verify chosen SVG/PNG destinations with the actual GPU renderer. Build first.
#>
[CmdletBinding()]
param()
. "$PSScriptRoot/Common.ps1"
$planEngine = Find-UnrealEngine
$planReportDir = Join-Path $RepoRoot ("Saved/Automation/PlanExportGPU-{0:yyyyMMdd-HHmmss}" -f (Get-Date))
New-Item -ItemType Directory -Force -Path $planReportDir | Out-Null
$planLogPath = Join-Path $planReportDir 'Automation.log'
$planArguments = @(
    "`"$UProjectPath`"", '-ExecCmds="Automation RunTests Floor101.Interface.PlanDestination"',
    '-TestExit="Automation Test Queue Empty"', "-ReportExportPath=`"$planReportDir`"",
    '-unattended', '-RenderOffscreen', '-dx12', '-DisablePython', '-nosplash', '-nosound', '-nopause',
    "-abslog=`"$planLogPath`""
)
$planProcess = Start-Process -FilePath $planEngine.EditorCmdExe -ArgumentList $planArguments -WindowStyle Hidden -PassThru
if (-not $planProcess.WaitForExit(180000)) { throw "PNG verification timed out; inspect process $($planProcess.Id)." }
$planReportPath = Join-Path $planReportDir 'index.json'
if (-not (Test-Path -LiteralPath $planReportPath)) { throw "No export report; inspect $planLogPath." }
$planReport = Get-Content -LiteralPath $planReportPath -Raw | ConvertFrom-Json
if ($planProcess.ExitCode -ne 0 -or $planReport.succeeded -ne 1 -or $planReport.failed -ne 0 -or $planReport.notRun -ne 0) {
    throw "Chosen SVG/PNG export failed; inspect $planReportPath and $planLogPath."
}
Write-Output "SVG/PNG destination, cancel and folder recall passed with DX12: $planReportDir"
