<#
.SYNOPSIS
    Smoke-tests the Unreal MCP server of a running editor (start it with .\Scripts\RunEditorMcp.ps1).
.DESCRIPTION
    Checks the MCP handshake, loopback-only binding, tool search meta-tools, required toolsets and one
    read-only editor operation (current level + actor listing). Calls are strictly sequential.
    Exit code 0 = every check passed. Read-only: it never mutates editor state.
.EXAMPLE
    .\Scripts\CheckMcp.ps1
#>
[CmdletBinding()]
param(
    [int]$Port = 8000,
    [string[]]$RequiredToolsets = @(
        'editor_toolset.toolsets.scene.SceneTools',
        'editor_toolset.toolsets.actor.ActorTools',
        'AutomationTestToolset.AutomationTestToolset',
        'EditorToolset.LogsToolset',
        'LiveCodingToolset'
    )
)

. "$PSScriptRoot\Common.ps1"
. "$PSScriptRoot\McpClient.ps1"

$url = "http://127.0.0.1:$Port/mcp"
$results = [System.Collections.Generic.List[pscustomobject]]::new()
function Add-Check([string]$Name, [bool]$Passed, [string]$Detail) {
    $results.Add([pscustomobject]@{ Check = $Name; Status = $(if ($Passed) { 'PASS' } else { 'FAIL' }); Detail = $Detail })
}
function Measure-Call([scriptblock]$Call) {
    $watch = [System.Diagnostics.Stopwatch]::StartNew()
    $value = & $Call
    return [pscustomobject]@{ Value = $value; Ms = $watch.ElapsedMilliseconds }
}
function Get-ToolReturnValue($Result) {
    $text = Get-McpToolText $Result
    if (-not $text) { return $null }
    return ($text | ConvertFrom-Json -Depth 64).returnValue
}

Write-Step "Checking Unreal MCP at $url"
try {
    $session = New-McpSession $url
}
catch {
    Write-Host "MCP server not reachable at ${url}: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host 'Start it with .\Scripts\RunEditorMcp.ps1'
    exit 1
}

try {
    Add-Check 'MCP handshake' ([bool]$session.SessionId -and $session.ProtocolVersion -eq $McpProtocolVersion) `
        "protocol=$($session.ProtocolVersion) session=$($session.SessionId)"

    $bindings = @(Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue |
        Select-Object -ExpandProperty LocalAddress -Unique)
    Add-Check 'Loopback-only listener' ($bindings.Count -gt 0 -and @($bindings | Where-Object { $_ -notin '127.0.0.1', '::1' }).Count -eq 0) `
        "listening on: $($bindings -join ', ')"

    $toolNames = @((Invoke-McpMethod $session 'tools/list').tools | ForEach-Object { $_.name })
    $metaTools = 'list_toolsets', 'describe_toolset', 'call_tool'
    Add-Check 'Tool search meta-tools' (@($metaTools | Where-Object { $_ -notin $toolNames }).Count -eq 0) ($toolNames -join ', ')

    $listing = Get-McpToolText (Invoke-McpTool $session 'list_toolsets')
    $toolsets = @($listing -split "`n" | Where-Object { $_ -match '^- ([\w.]+):' } | ForEach-Object { $Matches[1] })
    $missing = @($RequiredToolsets | Where-Object { $required = $_; -not ($toolsets | Where-Object { $_ -like "$required*" }) })
    Add-Check 'Required toolsets' ($missing.Count -eq 0) ("{0} toolsets; missing: {1}" -f $toolsets.Count, $(if ($missing) { $missing -join ', ' } else { 'none' }))

    $scene = 'editor_toolset.toolsets.scene.SceneTools'
    $level = Measure-Call { Get-ToolReturnValue (Invoke-McpTool $session 'call_tool' @{ toolset_name = $scene; tool_name = 'get_current_level'; arguments = @{} }) }
    Add-Check 'Read-only: get_current_level' ([bool]$level.Value) "$($level.Value) ($($level.Ms) ms)"

    $actors = Measure-Call { @(Get-ToolReturnValue (Invoke-McpTool $session 'call_tool' @{
                toolset_name = $scene; tool_name = 'find_actors'; arguments = @{ name = ''; tag = ''; collision_channels = @() } })) }
    $sample = ($actors.Value | Select-Object -First 4 | ForEach-Object { ($_.refPath -split '\.')[-1] }) -join ', '
    Add-Check 'Read-only: find_actors' ($actors.Value.Count -gt 0) "$($actors.Value.Count) actors ($($actors.Ms) ms): $sample ..."
}
finally {
    Close-McpSession $session
}

$results | Format-Table -AutoSize -Wrap | Out-Host
$failed = @($results | Where-Object Status -eq 'FAIL')
if ($failed.Count -gt 0) {
    Write-Host "$($failed.Count) MCP check(s) FAILED" -ForegroundColor Red
    exit 1
}
Write-Step 'All MCP checks passed (calls issued sequentially)'
exit 0
