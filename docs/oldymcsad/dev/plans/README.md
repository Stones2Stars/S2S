# Plans — the in-flight roadmap (what we INTEND / where we are)

> **Status:** roadmap index · Reference describes what the code **IS**; plans describe what we **INTEND**
> and where the work stands. Never blur them — a stale "we will…" read as "it does…" is a classic
> head-bite ([`../_meta/CONVENTIONS.md`](../_meta/CONVENTIONS.md) §3).

## Active roadmap
| Doc | What it tracks |
|---|---|
| [`../json-migration/`](../json-migration/README.md) | **The #428/#430 migration — a one-time project folder ([gone when done](../_meta/CONVENTIONS.md#9-one-time-project-folders--when-done-theyre-gone)).** Holds the XML→JSON + cascade-build *transient* roadmap: engine status, the [rename registry](../json-migration/migration-renames.md), entity order + per-entity curation, the demolition map, building conversion. The DESIGN is durable in [`../explanation/cascade-architecture.md`](../explanation/cascade-architecture.md) + [`../reference/cascade/`](../reference/cascade/); the migration folder is the *how-we-get-there* that evaporates. |
| [structural-cleanup.md](structural-cleanup.md) | Dead-code-XML pass + source-tree cleanup. The `Sources/` reorg has **already landed** (Engine/AI/Infos/Infrastructure/Defines/Tools/UI/…); the doc tracks done-vs-pending — the `CvInfos.h` umbrella retirement (177 sites) is the big remaining item. |

## Parked — out of active scope, KEPT for intent
[`parked/`](parked/README.md) holds initiatives that are **not the active cascade/info-handling work** but
are **kept, not retired** ([DEC-keep-unkilled-ideas](../architecture/decisions.md#dec-keep-unkilled-ideas)):
the AI-side design (the AI north-star, sea/animal/worker AI, fight-or-flight, unit-AI valuation), the
combat-model rework, multimap-zone, gamespeed-simplification. An agent on the active cascade work does **not**
need them. They are carried **as-is** (not rebuilt) and get the grounding treatment **when their initiative
activates**.

## Triaged
- The **detailed cascade design specs** are rebuilt (durable) into [`../reference/cascade/`](../reference/cascade/)
  (`enabler`, `modifier`, `tally`, `event-spine`, `shadow`, `data-model`, `fixed-point-and-scales`,
  `constructibility`) — every `[DEC-id]` points at a rebuilt home.
- The **one-time migration roadmap/registry docs** (`cascade-migration`, `migration-renames`,
  `migration-entity-ranking`, `building-cascade-conversion`, `cascade-engine-430`, `calc-emulator-spec`) now
  live in the ephemeral [`../json-migration/`](../json-migration/README.md) folder (gone when the migration completes).
  `cascade-modifier-inventory` was absorbed into `modifier.md`/`legacy-value-calc-map.md`;
  `cascade-known-discrepancies` + `state-mapping` were point-in-time sweeps — dropped, superseded by the
  per-system observability maps + `shadow.md`.
- The non-cascade initiatives (`specialist-rebalance`, `improvement-category-yields`, `team-buildings`,
  `global-warming-mod`, `unified-civilopedia`, `turn-time-optimization`, `codebase-bug-hunt`) are
  **parked** ([parked/](parked/README.md)); `derived-data-repository` + the **cross-entity inversion**
  approach are **superseded** ([`../architecture/superseded-ideas.md`](../architecture/superseded-ideas.md)).

## See also
- [`../README.md`](../README.md) — the comprehension map.
- [`../_meta/build-plan.md`](../_meta/build-plan.md) — the docs2 rebuild tracker itself.
