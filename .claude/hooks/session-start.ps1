# SessionStart hook (matcher: startup|clear|resume|compact).
# Prints the post-compaction reload digest, then the READ-FIRST directive listing every
# sessionStart read-gate's docs. Output becomes session context, so it is in front of the
# agent before it engages the user's first message (owner: "you can do it before accepting input").
# Routed through pwsh (UTF-8 clean) per the owner ruling (pwsh good, powershell.exe bad).

[Console]::OutputEncoding = [Text.Encoding]::UTF8

$claudeDir = Split-Path -Parent $PSScriptRoot          # .claude/hooks -> .claude
$digest    = Join-Path $claudeDir 'post-compaction-reload.txt'
$gatesDir  = Join-Path $claudeDir 'read-gates'

if (Test-Path -LiteralPath $digest) {
    Get-Content -Raw -LiteralPath $digest
    ""
}

$gateFiles = @(Get-ChildItem -LiteralPath $gatesDir -Filter '*.json' -ErrorAction SilentlyContinue)
$active = @()
foreach ($gf in $gateFiles) {
    try { $g = Get-Content -Raw -LiteralPath $gf.FullName | ConvertFrom-Json } catch { continue }
    if ($g.sessionStart -and $g.docs) { $active += , $g }
}

if ($active.Count -gt 0) {
    "==================================================================================="
    "READ-GATE -- your FIRST action this session is to READ these docs IN FULL, before"
    "responding to the user. You may read before engaging the request (owner ruling:"
    "'you can do it before accepting input'). This codebase is a tightly-coupled tangle;"
    "skipping a subsystem doc has repeatedly wrecked sessions, so the read is ENFORCED:"
    "editing a gated path is BLOCKED (PreToolUse deny) until every doc below was Read"
    "this session. Read-only search is fine pre-gate. (Gates: .claude/read-gates/)"
    ""
    foreach ($g in $active) {
        $label = if ($g.label) { $g.label } else { $g.subsystem }
        "  $label :"
        foreach ($d in $g.docs) { "    - $d" }
        ""
    }
    "Also re-read AGENTS.md + Sources/AGENTS.md. The docs are authoritative; never"
    "reconstruct the design from live code or a summary."
    "==================================================================================="
}
