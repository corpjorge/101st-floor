<#
.SYNOPSIS
    Starts Unreal Editor on Floor101 with the Unreal MCP server running, without any editor UI steps.
.DESCRIPTION
    Uses the command-line switches supported by UE 5.8 (-ModelContextProtocolStartServer,
    -ModelContextProtocolPort). The per-user bAutoStartServer setting is not needed and not versioned.
    Waits until the server answers an MCP initialize request and verifies it only listens on loopback.
.EXAMPLE
    .\Scripts\RunEditorMcp.ps1
    .\Scripts\RunEditorMcp.ps1 -GenerateClientConfig   # also (re)writes .mcp.json via the official editor command
#>
[CmdletBinding()]
param(
    [int]$Port = 8000,
    # Runs "ModelContextProtocol.GenerateClientConfig ClaudeCode" in the editor (writes/merges .mcp.json).
    [switch]$GenerateClientConfig,
    [int]$TimeoutSeconds = 1800,
    [switch]$NoWait
)

. "$PSScriptRoot\Common.ps1"
. "$PSScriptRoot\McpClient.ps1"

$url = "http://127.0.0.1:$Port/mcp"

if (Test-McpServer $url) {
    Write-Step "An MCP server is already answering at $url; not starting another editor."
    exit 0
}
$started = Get-Date
# An editor for this project that is still starting up (e.g. compiling shaders) is waited for, not duplicated.
$running = Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'" |
    Where-Object { $_.CommandLine -like "*$ProjectName.uproject*" -and $_.CommandLine -like '*-ModelContextProtocolStartServer*' } |
    Select-Object -First 1
if ($running) {
    Write-Step "Unreal Editor for $ProjectName is already starting (PID $($running.ProcessId)); waiting for MCP."
    $editor = Get-Process -Id $running.ProcessId
}
else {
    $occupied = Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue
    if ($occupied) {
        throw "Port $Port is already in use by PID $(@($occupied)[0].OwningProcess). Pass -Port <n>."
    }

    $engine = Find-UnrealEngine
    $arguments = @("`"$UProjectPath`"", '-ModelContextProtocolStartServer', "-ModelContextProtocolPort=$Port")
    if ($GenerateClientConfig) {
        $arguments += '-ExecCmds="ModelContextProtocol.GenerateClientConfig ClaudeCode"'
    }

    Write-Step "Unreal Engine $($engine.Version): $($engine.EditorExe)"
    Write-Step "Starting Unreal Editor with MCP on $url"
    $editor = Start-Process -FilePath $engine.EditorExe -ArgumentList $arguments -PassThru
    if ($NoWait) {
        Write-Step "Editor PID $($editor.Id) started; not waiting for MCP."
        exit 0
    }
}

# The first launch of a project compiles editor shaders and can take many minutes.
while (-not (Test-McpServer $url)) {
    if ($editor.HasExited) {
        throw "Unreal Editor exited (code $($editor.ExitCode)) before MCP became available. See Saved\Logs\$ProjectName.log"
    }
    if (((Get-Date) - $started).TotalSeconds -gt $TimeoutSeconds) {
        throw "MCP did not answer at $url within $TimeoutSeconds s (editor PID $($editor.Id) still running)."
    }
    Start-Sleep -Seconds 5
}

# Security: the server must only be reachable from this machine.
$bindings = @(Get-NetTCPConnection -LocalPort $Port -State Listen | Select-Object -ExpandProperty LocalAddress -Unique)
$exposed = @($bindings | Where-Object { $_ -notin @('127.0.0.1', '::1') })
if ($exposed.Count -gt 0) {
    throw "MCP port $Port is listening on non-loopback address(es): $($exposed -join ', '). Stop the editor."
}

Write-Step ("MCP ready at {0} after {1:N0}s (editor PID {2}, listening on {3})" -f
    $url, ((Get-Date) - $started).TotalSeconds, $editor.Id, ($bindings -join ', '))
if ($GenerateClientConfig) {
    $mcpJson = Join-Path $RepoRoot '.mcp.json'
    Write-Step ".mcp.json present: $(Test-Path $mcpJson) ($mcpJson)"
}
