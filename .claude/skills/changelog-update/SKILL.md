---
name: changelog-update
description: Digest git history into docs/CHANGELOG.md. Use when the user asks to
  update, refresh, or catch up the changelog/featurelist, or when preparing a
  release. Sweeps commits since the last-digested marker, drafts player/modder
  bullets for the visible changes, and advances the marker — the agent-digest
  safety net behind the append-in-the-same-commit convention.
---

# Update the changelog from git history

`docs/CHANGELOG.md` is the curated player/modder-facing record. It is maintained two ways:

1. **Primary (convention, AGENTS.md Git/delivery):** a commit whose change a player or modder
   would notice appends one bullet to the `## Unreleased` section in the SAME commit.
2. **This skill (safety net):** digest whatever slipped through, from git history.

The old C2C script that generated a changelog from commit messages is dead and is not revived —
commit subjects here are engineering statements, not changelog lines; the digest requires
judgment about player visibility, which is why this is an agent skill and not a script.

## Procedure

1. Read the `<!-- last-digested: <hash> -->` marker near the top of `docs/CHANGELOG.md`.
2. Sweep `git log --format="%h %s" <hash>..HEAD` (chunk if large; fan out cheap subagents for
   a long range, per the assemble-the-minions pattern).
3. For each commit, judge: would a PLAYER notice it in a session, or a MODDER at the data/API
   surface? Internal refactors, doc edits, and build tooling are EXCLUDED. Merge iterative
   commits on one feature into ONE bullet describing the end state.
4. Ground every bullet in an actual commit — never invent, never embellish. Anything specced
   but not implemented is marked `[spec — verify]` or left out.
5. Append the bullets under `## Unreleased` (Players vs Modders subsections), matching the
   document's existing tone: direct, concrete, no commit hashes in the body.
6. Advance the marker to the digested HEAD hash. Commit the changelog edit.

## Rules

- The marker is the ONLY state in the file; do not add dates-per-line or hash citations.
- Do not rewrite existing curated sections — the owner curates; this skill only appends to
  Unreleased and may dedupe against bullets already present.
- At a release, the owner (or an explicitly asked agent) promotes Unreleased into a titled
  section; this skill never does that on its own.
