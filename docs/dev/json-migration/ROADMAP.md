# Cascade JSON migration — ROADMAP (single source of truth)

> **Status:** active · **This file is the entry point.** Where things stand, what's next, and how we
> work — all here. Read this, not the transcript. Every other json-migration doc is detail under it.

## End goal

Replace the legacy XML-driven building / modifier / active-set logic with the JSON-data-driven
**cascade** (enabler → modifier → tally), verified at **parity** against legacy through the live
observability surface. The cascade specs (`cascade-engine-430.md`, `cascade-migration.md`,
`building-cascade-conversion.md`, `reference/cascade/*`) are the authoritative design and **stay**.

## Operating discipline (the system — so the work is verifiable without watching it)

1. **One isolated, verifiable component at a time.** No "4 things at once." Each task has a standalone
   pass/fail check; we finish and verify one before starting the next (Clean-Architecture isolation).
2. **Ground truth only.** Every claim is a number pulled from a live endpoint. Parity is **binary**
   (the zero-bar), never a percentage, never self-certified. We have all endpoints and all data — we
   **map, we do not guess.**
3. **State lives on disk.** This roadmap and each task's output artifact are the record. Compaction
   cannot lose them, and they are read instead of the transcript.

## Tasks (sequenced; each independently verifiable)

1. **State Extractor** — pull the live endpoints → write **one readable JSON snapshot into a gitignored
   dir**. Extraction only: no calc, no parity, no enabler logic in it.
   *Done when:* the JSON snapshot is complete and openable, and its contents reconcile to the endpoints.
2. **Curated JSON data completeness** — every explicit edge (`grants` / `enables` / `requires`) present
   and correct against the source XML; **no reliance on identity fields**.
   *Done when:* each edge traces to a named source-XML fact (audited, not asserted).
3. **Cascade (C++) parity vs legacy** — the end goal, verified through the live shadow / diagnostic
   surface, driven by the cascade specs.
   *Done when:* the per-mechanic zero-bar holds (`DEC-per-mechanic-parity`).

## Dropped — "did not happen"

- **Dry-calc emulator:** `Tools/ModifierCalc/` (Python) + `calc-emulator-spec.md` +
  `calc-live-parity-findings.md`. It entangled extractor + enabler + modifier + parity into one tangle
  (the "4 things at once" failure). Codebase learnings are **extracted into the durable cascade/engine
  refs first**, then the emulator + its two docs are removed and references stripped.
- **Read-gate enforcement** (the PreToolUse deny + SessionStart doc-listing hooks) — wildly unreliable,
  false security; **removed entirely** — both hooks nuked, no replacement. Git history is the record.

## Kept

Cascade specs · curators (`Tools/Migration/`) · curated JSON data (`Assets/Data/`) · the live
observability surface (all endpoints).
