# Minimal MCP (Streamable HTTP, 2025-06-18) client used by the Unreal MCP scripts.
# Dot-source it: . "$PSScriptRoot\McpClient.ps1"
# Requests are strictly sequential: Unreal MCP tool calls must never overlap (see docs/unreal-mcp.md).
Set-StrictMode -Version Latest

$script:McpProtocolVersion = '2025-06-18'

function ConvertFrom-McpResponse($Response) {
    $contentType = [string]($Response.Headers['Content-Type'] | Select-Object -First 1)
    $text = [string]$Response.Content
    if ($contentType -like 'text/event-stream*') {
        # Progress-enabled responses arrive as SSE; the final JSON-RPC message is the last "data:" line.
        $text = ($text -split "`n" | Where-Object { $_ -like 'data:*' } | Select-Object -Last 1) -replace '^data:\s*', ''
    }
    if ([string]::IsNullOrWhiteSpace($text)) { return $null }
    return $text | ConvertFrom-Json -Depth 64
}

function Send-McpPayload($Session, [hashtable]$Payload, [int]$TimeoutSec = 120) {
    $headers = @{ Accept = 'application/json, text/event-stream'; 'MCP-Protocol-Version' = $McpProtocolVersion }
    if ($Session.SessionId) { $headers['Mcp-Session-Id'] = $Session.SessionId }
    $body = $Payload | ConvertTo-Json -Depth 64 -Compress
    Invoke-WebRequest -Uri $Session.Url -Method Post -Body $body -ContentType 'application/json' -Headers $headers `
        -TimeoutSec $TimeoutSec -SkipHttpErrorCheck
}

function New-McpSession([string]$Url, [int]$TimeoutSec = 10) {
    $session = [pscustomobject]@{ Url = $Url; SessionId = $null; NextId = 1; ServerInfo = $null; ProtocolVersion = $null }
    $response = Send-McpPayload $session -TimeoutSec $TimeoutSec -Payload @{
        jsonrpc = '2.0'; id = 0; method = 'initialize'
        params  = @{ protocolVersion = $McpProtocolVersion; capabilities = @{}; clientInfo = @{ name = '101st-floor-scripts'; version = '1.0' } }
    }
    if ($response.StatusCode -ne 200) { throw "MCP initialize failed: HTTP $($response.StatusCode) $($response.Content)" }
    $message = ConvertFrom-McpResponse $response
    if ($message.PSObject.Properties['error']) { throw "MCP initialize error: $($message.error.message)" }

    $session.SessionId = [string]($response.Headers['Mcp-Session-Id'] | Select-Object -First 1)
    $session.ServerInfo = $message.result.serverInfo
    $session.ProtocolVersion = $message.result.protocolVersion
    Send-McpPayload $session @{ jsonrpc = '2.0'; method = 'notifications/initialized' } | Out-Null
    return $session
}

function Invoke-McpMethod($Session, [string]$Method, [hashtable]$Params = @{}, [int]$TimeoutSec = 120) {
    $id = $Session.NextId++
    $response = Send-McpPayload $Session -TimeoutSec $TimeoutSec -Payload @{ jsonrpc = '2.0'; id = $id; method = $Method; params = $Params }
    if ($response.StatusCode -ne 200) { throw "MCP $Method failed: HTTP $($response.StatusCode) $($response.Content)" }
    $message = ConvertFrom-McpResponse $response
    if ($message.PSObject.Properties['error']) { throw "MCP $Method error $($message.error.code): $($message.error.message)" }
    return $message.result
}

# Calls a native MCP tool and returns its result (content[] / structuredContent / isError).
function Invoke-McpTool($Session, [string]$Name, [hashtable]$Arguments = @{}) {
    Invoke-McpMethod $Session 'tools/call' @{ name = $Name; arguments = $Arguments }
}

# Concatenated text content of a tool result.
function Get-McpToolText($Result) {
    (@($Result.content) | Where-Object { $_.type -eq 'text' } | ForEach-Object { $_.text }) -join "`n"
}

function Close-McpSession($Session) {
    if (-not $Session.SessionId) { return }
    Invoke-WebRequest -Uri $Session.Url -Method Delete -Headers @{ 'Mcp-Session-Id' = $Session.SessionId } `
        -TimeoutSec 10 -SkipHttpErrorCheck | Out-Null
}

function Test-McpServer([string]$Url) {
    try {
        $session = New-McpSession $Url -TimeoutSec 5
        Close-McpSession $session
        return $true
    }
    catch {
        return $false
    }
}
