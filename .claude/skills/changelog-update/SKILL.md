---
name: changelog-update
description: Digest git history into docs/CHANGELOG.md. Use when the user asks to
  update, refresh, or catch up the changelog/featurelist, or when preparing a
  release. Sweeps commits since the last-digested marker, drafts player/modder
  bullets for the visible changes, and advances the marker — the ONLY way the
  file is written.
---

# Update the changelog from git history

`docs/CHANGELOG.md` is a **release feature list, GENERATED when one is wanted** — not a file
maintained per commit. This skill is the only thing that writes it.

⛔ **The append-in-the-same-commit convention is RETIRED** (AGENTS.md Git/delivery): every branch
appended at the same `## Unreleased` anchor, so any two concurrent branches conflicted by
construction. Ordinary commits must not touch the file at all.

⇒ **The player-facing story lives in the COMMIT BODY**, which is where the author's context was,
and this digest reads it there. ⚠ Commit SUBJECTS are engineering statements and are not
changelog lines — the old C2C subject-derived script is dead and is not revived. Read bodies
(`git log --format="%h %s%n%b"`), not subjects alone; the digest requires judgment about player
visibility, which is why this is an agent skill and not a script.

## Procedure

1. Read the `<!-- last-digested: <hash> -->` marker near the top of `docs/CHANGELOG.md`.
2. Sweep `git log --format="%h %s%n%b" <hash>..HEAD` — **bodies included, which is where the
   player-facing account is**; a subject alone cannot be judged from (chunk if large; fan out
   cheap subagents for a long range, per the assemble-the-minions pattern).
3. For each commit, judge: would a PLAYER notice it in a session, or a MODDER at the data/API
   surface? Internal refactors, doc edits, and build tooling are EXCLUDED. Merge iterative
   commits on one feature into ONE bullet describing the end state. ⚠ A MERGE commit carries no
   body of its own — read the commits it brought in, not the merge subject.
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
