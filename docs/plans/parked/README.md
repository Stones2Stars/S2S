# Parked — out-of-active-scope plans, kept for intent

> **Status:** parked partition (carried as-is, NOT rebuilt) · **Policy:**
> [the keep-unkilled-ideas policy](#parked--out-of-active-scope-plans-kept-for-intent).

These are design initiatives **outside the active cascade / info-handling scope**. They are **kept, not
retired** — forward design intent is not reconstructible from code, so being merely out-of-scope is never a
reason to drop it. They are **not** superseded/killed (those go to
[`../../architecture/superseded-ideas.md`](../../architecture/superseded-ideas.md)); they are simply *parked*.

## ⚠ These are carried AS-IS — not yet rebuilt to the docs2 grounding standard
They came straight from the old `docs/plans/` set, so they may carry **stale paths** (the `Sources/`
reorg) and **stale status**. **Do not trust their detail as current.** Each gets the full grounding +
consolidation treatment **when its initiative becomes active** — at which point it moves into the active
[`../README.md`](../README.md) roadmap. Until then it preserves the *intent* only.

## What's here
**Triaged 2026-06-23** — the implemented/exhausted plans were dropped (`size-matters-ai`, `gamespeed-simplification`,
`combat-odds-baseline`, `combat-phase3b-plan`). Several below are **PARTIAL**: their done phases are noted inside; the
open remainder is the live item.

- **AI side** — `ai-architecture-north-star`, `ai-vs-human-benchmarking`,
  `ai-build-queue-parity`, `unit-ai-valuation`, `sea-ai-rework`, `subdued-animal-ai`.
  *(The AI is the consumer of the cascade data side — [`../../architecture/north-star.md`](../../architecture/north-star.md) §1.)*
- **Combat model** — `combat-model-sketch` (air-combat / Layer-2 gaps), `combat-simplification-scope`
  (the good-ideas backlog), `fight-or-flight` (preserved for pluggable reimplementation).
- **Systems / data** — `improvement-category-yields`, `specialist-rebalance`,
  `global-warming-mod` (the #436 vestige-removal scope), `post-migration-content-purge` (content reclassification
  deferred until after the #428/#430 migration completes), `astrological-ancient-way-traits` (a cut trait/wonder
  system kept for reimplementation), `ranked-target-selection` (design locked in `json.md` §3.3, implementation
  pending), `upgrade-chains` (building tiers as a first-class chain rather than the implicit inverse of a
  dormancy list; the ruling banning building→building obsolescence is already LANDED in `enabler.md` §2),
  `inflation-remodel` (inflation is not used in the game and #430 does not remodel it; when it returns
  it is a cascade channel driven by ACTUAL EXPENDITURE, and that plan does not exist yet).
- **Performance / other** — `turn-time-optimization`, `codebase-bug-hunt`, `worker-stranded-tiles-reachability`,
  `surround-destroy-removal-map`, `multimap-zone-rework`, `unified-civilopedia`.
- **Modules** — `module-system-rework` (the inherited C2C module mechanism is not the future one; also records
  why the curator's module exclusions are an intentional boundary, so the dangling refs they leave are not
  chased as curator gaps).

## See also
- [`../README.md`](../README.md) — the active roadmap.
- [`../../architecture/superseded-ideas.md`](../../architecture/superseded-ideas.md) — the *killed* ideas (parked ≠ killed).
