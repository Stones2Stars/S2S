# Read-gates — enforced "read the docs before you touch the code"

This codebase is a tightly-coupled, decades-deep Civ4/BTS/C2C tangle. **Skipping the subsystem
docs has repeatedly wrecked agentic sessions** (an assumption reconstructed from stale code or a
summary, then built on). Exhortation alone ("read the docs") has failed — it leaves the
stop-condition to the agent's judgement, which is biased toward getting to the task. A read-gate
removes that judgement: it makes the read **mechanically unavoidable** before any edit lands.

Owner ruling (2026-06-19) — full statement in `AGENTS.md` Conventions ("Read-gates").

## What a gate is

One JSON file per subsystem. Each declares the docs that MUST be read in a session before code
in that subsystem may be edited, and the code paths that trigger the gate:

```json
{
  "subsystem": "cascade",
  "label": "human-readable name (shown in the session-start directive)",
  "sessionStart": true,                       // list these docs at every session start (open/clear/resume/compact)
  "docs":  [ "docs/dev/plans/foo.md", ... ],  // repo-relative; must ALL be Read this session
  "paths": [ "Sources/Cascade/**", ... ]      // editing a file under any of these triggers the gate
}
```

## How it is enforced (two hooks, `.claude/hooks/`, wired in `.claude/settings.json`)

- **`session-start.ps1`** (`SessionStart`, matcher `startup|clear|resume|compact`) prints the
  post-compaction reload digest, then lists the docs of every `sessionStart: true` gate with a
  directive: *your first action this session is to Read each of these in full.* You read before
  engaging the user's request (owner: "you can do it before accepting input").
- **`read-gate.ps1`** (`PreToolUse`, matcher `Edit|Write|MultiEdit`) parses the session transcript
  for `Read` calls; if the file being edited sits under a gate's `paths` and any of that gate's
  `docs` was not Read this session, the edit is **denied (exit 2)** with the unread list. It
  **fails OPEN** on its own errors (printing a loud warning), so a hook bug never bricks editing —
  but a fail-open is visible, never silent.

The gate proves the docs were *opened*, not *understood* — comprehension can't be automated. But
"never opened them" is the failure that keeps happening, and that is fully gateable.

## Minion exception (owner ruling 2026-06-19)

The gate targets the **orchestrator** (the agent with broad edit authority that must hold the
whole subsystem design). A spawned **sub-agent / minion** only needs the context it OPERATES
UNDER: the orchestrator (which has read the docs) briefs it with exactly the slice it needs and
owns its correctness, so a minion is **exempt from the full-manifest read** — forcing it to re-read
every doc defeats the point of delegating to a cheap, scoped context. The session-start read-first
directive is for the main session. (Project minions today — `data-reader`, `Explore` — are
read-only, so they don't hit the `Edit` gate. If an edit-capable minion on gated paths ever
becomes real: brief it to Read the one doc its slice needs, or refine `read-gate.ps1` to recognize
sub-agent context — a verify-then-change follow-up; the hook stdin's sub-agent signal is unconfirmed.)

## Adding a subsystem

Drop a new `<subsystem>.json` here with its `docs` + `paths`. No hook edits needed — both hooks
scan this directory. Set `sessionStart: true` only for a subsystem that is the branch's active
core work (so a fresh session is told to read it up front); leave it off for gates that should
fire only when their paths are actually touched.
