# Parked — out-of-active-scope plans, kept for intent

> **Status:** parked partition (carried as-is, NOT rebuilt) · **Policy:**
> [DEC-keep-unkilled-ideas](../../architecture/decisions.md#dec-keep-unkilled-ideas).

These are design initiatives **outside the active cascade / info-handling scope**. They are **kept, not
retired** — forward design intent is not reconstructible from code, so being merely out-of-scope is never a
reason to drop it. They are **not** superseded/killed (those go to
[`../../architecture/superseded-ideas.md`](../../architecture/superseded-ideas.md)); they are simply *parked*.

## ⚠ These are carried AS-IS — not yet rebuilt to the docs2 grounding standard
They came straight from the old `docs/dev/plans/` set, so they may carry **stale paths** (the `Sources/`
reorg) and **stale status**. **Do not trust their detail as current.** Each gets the full grounding +
consolidation treatment **when its initiative becomes active** — at which point it moves into the active
[`../README.md`](../README.md) roadmap. Until then it preserves the *intent* only.

## What's here
- **AI side** — `ai-architecture-north-star`, `ai-logging-rollout`, `ai-vs-human-benchmarking`,
  `unit-ai-valuation`, `sea-ai-rework`, `size-matters-ai`, `subdued-animal-ai`, `fight-or-flight`.
  *(The AI is the consumer of the cascade data side — [`../../architecture/north-star.md`](../../architecture/north-star.md) §1.)*
- **Combat model rework** — `combat-model-sketch`, `combat-odds-baseline`, `combat-phase3b-plan`,
  `combat-simplification-scope`.
- **Other** — `worker-stranded-tiles-reachability`, `surround-destroy-removal-map`, `multimap-zone-rework`,
  `gamespeed-simplification`.

## See also
- [`../README.md`](../README.md) — the active roadmap.
- [`../../architecture/superseded-ideas.md`](../../architecture/superseded-ideas.md) — the *killed* ideas (parked ≠ killed).
