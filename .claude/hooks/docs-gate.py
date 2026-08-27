#!/usr/bin/env python
"""The DOCS GATE: block the FIRST Sources/ CODE edit of every session until the agent has acknowledged
reading the governing docs. READ THE DOCS, ASK WHEN YOU DO NOT KNOW, DO NOT ASSUME.
PreToolUse hook on Edit|Write; per-session marker under .claude/docs-ack/ (gitignored)."""
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
    if fp.lower().endswith((".md", ".txt")):
        return  # Sources/AGENTS.md is a DOC, not source -- gating it blocks doc work on the docs rule
    claude_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # <repo>/.claude/hooks -> <repo>/.claude
    marker = os.path.join(claude_dir, "docs-ack", sid)
    if os.path.exists(marker):
        return  # acknowledged this session
    reason = (
        "DOCS GATE - first Sources/ edit this session is BLOCKED. Before ANY source edit: "
        "(1) READ the governing docs for the subsystem you are touching, END TO END. "
        "docs/README.md is the index; a large concept's home is a DIRECTORY with a hub page "
        "(docs/cascade.md is a map -- the spec is the pages in docs/cascade/), and the hub "
        "carries no ruling, so stopping at it is reading the contents page. "
        "(2) STATE the design back from the spec in your own words in your reply - if you cannot, you have "
        "not read it; (3) then create the marker file '.claude/docs-ack/" + sid + "' and retry the edit. "
        "READ THE DOCS, ASK WHEN YOU DO NOT KNOW, AND ABOVE ALL DO NOT ASSUME."
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
