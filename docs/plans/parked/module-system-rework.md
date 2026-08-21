# Module / modmod system — a fresh mechanism (parked intent)

> **Status:** parked partition · **Policy:**
> [the keep-unkilled-ideas policy](README.md#parked--out-of-active-scope-plans-kept-for-intent).

**S2S does not use the inherited C2C module mechanism as a live extension system any more, and it is not the
shape the future one takes.** A new way to handle **modules and modmods** is intended; its design is open. This
doc exists so the current, deliberately-narrow handling is not mistaken for a gap and "fixed" by an agent.

## Current truth — the curator decides what is IN

The **curator folds in the modules we want**: `Tools/Migration/store.py` enumerates `Assets/XML` **and**
`Assets/Modules`, so a wanted module's records merge into the curated `Assets/Data/**` exactly like base XML.
What is not wanted is named in **`EXCLUDED_MODULE_SUBPATHS`** (today: `zwip`, `bad_karma`, `p2k_multimaps_test`)
and is simply never ingested — **an intentional content boundary, not a curator coverage gap.**

**Consequence to expect (not a defect to chase).** An excluded module's *migrated-category* records (its units,
improvements, …) get no curated JSON and therefore **no engine id**, while its records in categories that are
still XML-loaded (event triggers, spawns, …) are loaded by the engine and can name those absent ids. The result
is a dangling reference — visible as an `Xml_MissingTypes.log` line, or as a spawn/trigger that silently never
fires. Known instances: `IMPROVEMENT_INDIGENOUS_COMMUNITY` (`zWIP/ExtraDiplomacy`, named by a loaded event
trigger and by `Assets/Python/DancingHoskuld/BarbarianDiplomacy.py`) and `UNIT_KILLERRABBIT`
(`Bad_Karma/Fantasy`, named by a loaded `CIV4SpawnInfos.xml`).

⛔ **Do NOT "fix" these by adding the module to the curator** — that re-admits content deliberately left out.
They are artifacts of dead content whose home mechanism is being replaced wholesale; they resolve when the
module system is redone (or when the dead content is purged —
[post-migration-content-purge.md](post-migration-content-purge.md)).

## What the rework has to decide

- How a module/modmod declares itself, and what may be overridden vs added (the legacy `bForceOverwrite` /
  `copyNonDefaults` merge is the inherited answer, not the intended one).
- Where module content lives relative to `Assets/Data/**` — merged at curation (offline, as today) vs loaded as
  its own layer at runtime. Note the standing boundary: the **game does not know curation exists**
  ([curators/README.md](../../specs/curators/README.md)), so a runtime layer is a genuine model change.
- How an excluded/absent module's dangling references fail — loudly at load, or pruned.

## See also
- [`../../specs/curators/README.md`](../../specs/curators/README.md) — the curator conversion spec (module fold-in
  is a store-level concern). · [`post-migration-content-purge.md`](post-migration-content-purge.md) — the dead-content pass.
