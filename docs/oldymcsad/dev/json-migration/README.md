# JSON migration — the one-time #428/#430 XML→JSON + cascade-build roadmap

> **Status:** roadmap / working docs · **Lifecycle:** this whole folder is a **ONE-TIME event.** It is
> front-and-center while the #428/#430 migration is in flight; **when the migration is done the whole folder
> is ARCHIVED OUT of the active docs scope** (moved out like `old-docs/`, **not deleted** — referenceable
> later; owner ruling 2026-06-20). It is therefore NOT held to the durable
> grounding standard of [`../_meta/CONVENTIONS.md`](../_meta/CONVENTIONS.md) the way `reference/` is — these
> are live curator/build working docs, and some internal cross-links may still reference retired specs.

The **durable** cascade design lives in [`../explanation/cascade-architecture.md`](../explanation/cascade-architecture.md)
+ [`../reference/cascade/`](../reference/cascade/) (enabler / modifier / tally / event-spine / data-model /
fixed-point / shadow / constructibility). **This folder is the opposite:** the *transient* mechanics of
getting from the legacy XML to the new JSON + standing up the cascade engine, which evaporate when the job is
done.

## What's here

| Doc | What it is |
|---|---|
| [`TO-BE-MADE-DURABLE.md`](TO-BE-MADE-DURABLE.md) | **the archive gate** — the running checklist of durable knowledge in this folder that must be lifted into the durable set (`reference/`/`explanation/`/ledger) before this folder is archived out. Append a pointer row as-you-go; the folder is archived only when this is empty. |
| [`cascade-migration.md`](cascade-migration.md) | the migration roadmap / index — engine build status, entity order, demolition map |
| [`migration-renames.md`](migration-renames.md) | **the rename registry** — the exhaustive old-XML-tag → new-JSON-key map per entity. The lookup surface `readJson` + modders consult; the durable docs that say "the rename registry (plans)" point HERE. |
| [`infotype-translation.md`](infotype-translation.md) | **the four-layer Rosetta** ("the real takeback") — per legacy field, the map across old XML · legacy-C++ MEANS · new JSON key/predicate/target · consuming cascade machine. The durable scope-limiter for curator/readJson/cascade work; seeded from the rename registry's predicate-rewiring section. **Drafted across all 33 entities** (~1367 rows; Section 1 plot-substrate + Sections 2–6 by tier) with a **needs-owner-ruling appendix** (machine-label calls, stale-doc drifts, real un-migrated gaps). |
| `migration-entity-ranking.md` — **ARCHIVED** (→ `old-docs/json-migration/`, 2026-06-20) | the XML→JSON entity migration ORDER + per-entity curation decisions. The migration is **complete**, so this is a done record; its durable substance lives in `Tools/Migration/classifications/*.json` + `store.py` + the specs (`cascade-migration` / `cascade-engine-430`). Archived out of active scope (consult-by-need). |
| [`building-cascade-conversion.md`](building-cascade-conversion.md) | the per-building conversion map (the CREST / deliveryguy decisions resolved during curation) |
| [`cascade-engine-430.md`](cascade-engine-430.md) | the #430 engine build plan (build order, slices, the BoolExpr-reuse + CvDerivedData-skeleton-deferral decisions, the readJson `--render` tool leg) |
| [`calc-emulator-spec.md`](calc-emulator-spec.md) | ⚠ **SEMI-RETIRED.** Emulating the full *legacy* calc pipeline offline was abandoned — it was easier to **dump the individual calcs from the game itself** ([`../reference/cascade/legacy-value-calc-map.md`](../reference/cascade/legacy-value-calc-map.md) §12). The **cascade calc emulator** (`Tools/ModifierCalc/cascade_sim.py`, the combine-only comparator) **is still used** and is documented live in [`../reference/cascade/shadow.md`](../reference/cascade/shadow.md) §7. Kept here as history of the one-time effort. |

## Tools (the corresponding migration tools live under `Tools/`)

- **`Tools/Migration/`** — the XML→JSON **curators** (`curate_<entity>.py`, `store.py`, `engine.py`). Operational runner reference: [`../../../Tools/Migration/README.md`](../../../Tools/Migration/README.md).
- **`Tools/ModifierCalc/`** — the offline calculators: **`dry_calc.py`** (the primary zero-ride-in full
  per-scope calc — every economic channel from JSON + fictitious state, validated in isolation;
  [DEC-calc-zero-ride-in](../architecture/decisions.md#dec-calc-zero-ride-in)) and **`cascade_sim.py`** (the
  same-input-vector combine comparator). Both doc'd live in [`../reference/cascade/shadow.md`](../reference/cascade/shadow.md) §7.
- **`Tools/ReadJson/`** — **`readJson`**, the tool that outputs a human-readable summary of any item defined in the JSON. It is a **TOOL**, slated to be standardized as a **modding tool** so a user can see, out-of-game, what their changes actually do (owner 2026-06-20). Not migration-specific — it outlives the migration.

## See also
- [`../README.md`](../README.md) — the comprehension map.
- [`../explanation/cascade-architecture.md`](../explanation/cascade-architecture.md) — the durable cascade design this migration builds toward.
- [`../_meta/build-plan.md`](../_meta/build-plan.md) §4 — the docs-rebuild swap + `old-docs/` retirement (the pre-rebuild set, the diff source for parity).
