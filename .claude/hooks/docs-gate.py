#!/usr/bin/env python
"""The DOCS GATE (owner mandate 2026-07-04): block the FIRST Sources/ edit of every session until the
agent has acknowledged reading the governing docs. 'READ THE DOCS, ASK WHEN YOU DO NOT KNOW, DO NOT
ASSUME.' PreToolUse hook on Edit|Write; per-session marker under .claude/docs-ack/ (gitignored)."""
import sys, json, os

def main():
    try:
        d = json.load(sys.stdin)
    except Exception:
        return  # unparseable input -> never block on a gate malfunction
    fp = str((d.get("tool_input") or {}).get("file_path", "")).replace("\\", "/")
    sid = str(d.get("session_id", "")) or "unknown-session"
    if "/Sources/" not in fp and not fp.startswith("Sources/"):
        return  # only source edits are gated
    claude_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # <repo>/.claude/hooks -> <repo>/.claude
    marker = os.path.join(claude_dir, "docs-ack", sid)
    if os.path.exists(marker):
        return  # acknowledged this session
    reason = (
        "DOCS GATE - first Sources/ edit this session is BLOCKED. Before ANY source edit: "
        "(1) READ the governing docs for the subsystem you are touching, END TO END "
        "(the relevant docs/specs/*.md + docs/plans/*.md + docs/architecture/state-repositories.md); "
        "(2) STATE the design back from the spec in your own words in your reply - if you cannot, you have "
        "not read it; (3) then create the marker file '.claude/docs-ack/" + sid + "' and retry the edit. "
        "READ THE DOCS, ASK WHEN YOU DO NOT KNOW, AND ABOVE ALL DO NOT ASSUME (owner, 2026-07-04)."
    )
    print(json.dumps({
        "hookSpecificOutput": {
            "hookEventName": "PreToolUse",
            "permissionDecision": "deny",
            "permissionDecisionReason": reason,
        }
    }))

if __name__ == "__main__":
    main()
