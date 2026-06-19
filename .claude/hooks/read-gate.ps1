# PreToolUse hook (matcher: Edit|Write|MultiEdit).
# Enforces the read-gates (.claude/read-gates/*.json): if the file being edited sits under a
# gate's `paths`, every one of that gate's `docs` must have been Read in THIS session (proven
# from the transcript) or the edit is DENIED (exit 2). The actual recurring failure in this repo
# is "never opened the doc"; that is what this catches. Comprehension is not automatable.
#
# Fails OPEN on its own errors (a hook bug must never brick editing) -- but loudly, never silently,
# so a fail-open is visible and can be fixed. Routed through pwsh per the owner ruling.

try {
    $raw = [Console]::In.ReadToEnd()
    if (-not $raw) { exit 0 }
    $inp = $raw | ConvertFrom-Json

    $tool = [string]$inp.tool_name
    if ($tool -notin @('Edit', 'Write', 'MultiEdit')) { exit 0 }

    $fp = [string]$inp.tool_input.file_path
    if (-not $fp) { exit 0 }

    # Normalize the edited path to a repo-relative, forward-slash, lowercase string.
    $cwd  = if ($inp.cwd) { [string]$inp.cwd } else { (Get-Location).Path }
    $norm = ($fp  -replace '\\', '/')
    $cwdn = ($cwd -replace '\\', '/').TrimEnd('/')
    $rel  = $norm
    if ($norm.ToLower().StartsWith($cwdn.ToLower() + '/')) {
        $rel = $norm.Substring($cwdn.Length).TrimStart('/')
    }
    $relL = $rel.ToLower()

    $claudeDir = Split-Path -Parent $PSScriptRoot
    $gatesDir  = Join-Path $claudeDir 'read-gates'
    $gateFiles = @(Get-ChildItem -LiteralPath $gatesDir -Filter '*.json' -ErrorAction SilentlyContinue)

    # Which gates does this edit trigger? (path prefix match; trailing * / ** stripped.)
    $matched = @()
    foreach ($gf in $gateFiles) {
        try { $gate = Get-Content -Raw -LiteralPath $gf.FullName | ConvertFrom-Json } catch { continue }
        foreach ($p in $gate.paths) {
            $prefix = (([string]$p) -replace '\*+$', '').Replace('\', '/').TrimEnd('/').ToLower()
            if ($prefix -and ($relL -eq $prefix -or $relL.StartsWith($prefix + '/'))) { $matched += , $gate; break }
        }
    }
    if ($matched.Count -eq 0) { exit 0 }

    # Collect every file Read this session, from the transcript.
    $readSet = New-Object System.Collections.Generic.HashSet[string]
    $tp = [string]$inp.transcript_path
    if ($tp -and (Test-Path -LiteralPath $tp)) {
        foreach ($line in [System.IO.File]::ReadLines($tp)) {
            if ($line.Length -lt 2 -or $line -notmatch '"Read"') { continue }   # fast pre-filter
            try { $o = $line | ConvertFrom-Json } catch { continue }
            $content = $o.message.content
            if (-not $content) { continue }
            foreach ($b in $content) {
                if ($b.type -eq 'tool_use' -and $b.name -eq 'Read' -and $b.input.file_path) {
                    [void]$readSet.Add((([string]$b.input.file_path) -replace '\\', '/').ToLower())
                }
            }
        }
    }

    # Any gated doc not Read this session?
    $unread = New-Object System.Collections.Generic.List[string]
    foreach ($gate in $matched) {
        foreach ($doc in $gate.docs) {
            $docL = (([string]$doc) -replace '\\', '/').ToLower()
            $found = $false
            foreach ($k in $readSet) { if ($k.EndsWith($docL)) { $found = $true; break } }
            if (-not $found -and -not $unread.Contains($doc)) { $unread.Add($doc) }
        }
    }
    if ($unread.Count -eq 0) { exit 0 }

    $lines = $unread | ForEach-Object { "  - $_" }
    $msg = @"
READ-GATE BLOCK: editing '$rel' requires reading this subsystem's docs first, this session.
This repo punishes assumptions -- a skipped doc has repeatedly wrecked sessions. Read each of
these IN FULL (the Read tool), then retry the edit:
$($lines -join "`n")
(Enforced by .claude/read-gates + .claude/hooks/read-gate.ps1; owner ruling -- AGENTS.md Conventions.)
"@
    [Console]::Error.WriteLine($msg)
    exit 2
}
catch {
    [Console]::Error.WriteLine("read-gate hook error (FAILING OPEN -- gate NOT enforced this call): $($_.Exception.Message)")
    exit 0
}
