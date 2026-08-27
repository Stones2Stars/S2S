# Curators — the migration conversion spec (transient)

> **Project-specific & temporary.** The migration curators (`Tools/Migration/curate_*.py`)
> convert the legacy Civ4 XML into the clean JSON shapes the cascade reads. This area specs **what the curators do** —
> the per-entity conversion decisions and the de-scale registry. (The **old→new field map is NOT a doc** — it lives
> in the curators themselves; see below.)
>
> **Separate from [json.md](../json.md) on purpose:** json.md is the **durable** spec of the JSON *shape* (what the
> data IS); this area is **how the data got there** (transient). It is **dropped when the migration completes** — do
> NOT fold it into the durable JSON spec.
>
> ⛔ **THE CURATOR IS BEING GOT RID OF, BECAUSE THE XML DOES NOT STAY.** The legacy XML is curator INPUT and
> nothing else; when it goes, the curator has no input and stops existing. ⇒ **`Assets/Data/**` then becomes the
> AUTHORED SOURCE rather than a derived artifact**, and the `_additions/` overlay dissolves into it — an addition
> is only an overlay because something downstream regenerates over it. ⛔ So do not build NEW dependence on the
> curator, and do not treat "the curator does it" as an answer to where data should live. ⚠ Until the XML is
> actually gone the current rules bind exactly as written: `Assets/Data/**` is DERIVED and never hand-edited, and
> gameplay data authors in `_additions/`.
>
> ⚖ **ONE FILE IN THIS FOLDER IS THE EXCEPTION: `fixed-point-and-scales.md` does NOT drop.** It is the permanent
> home of the ×100 fixed-point model and the curator-owns-descale rule — the ×100 scale MODEL and its per-field
> registry are durable rulings, not a migration artifact, even though the file happens to live beside the
> transient curator specs. It stays after every other file here is dropped.
>
> *(These were lifted intact from the old migration set rather than condensed: they are transient working specs, so
> preserve-and-place beats a condensing investment. Their internal links still point at pre-move paths — part of the
> global reference sweep follow-up.)*

## The old→new map lives in the curators
There is **no rename-trail / infotype-translation doc** — the old→new field map **is** the curator code, and that is
where it stays (a doc copy poisons context and drifts). Each `curate_<entity>.py` **docstring annotates every new key
to its legacy field, with the why**, and the code right below implements it. Canonical exemplar —
`curate_gamespeed.py`:

> `speed.world.percent` = `iSpeedPercent` — the master game-pace percentage … · `missionYieldMultiplier.world.percent`
> = `iUnitYieldScalePercent` …

The mechanical de-Hungarianization (`iX` → `x`) lives in `engine.py`; the per-entity semantic renames live in each
curator's docstring + body. To read the map for an entity, **read its curator.**

## Post-curation additions (`curate_additions.py`) — the hand-authored layer

> **Entity curation is complete, so new GAMEPLAY data no longer goes in the legacy XML
> (curator input) — it is a POST-CURATION ADDITION.** Additions author in `Assets/Data/_additions/<type>.json`
> (an entity id → a partial object) and `curate_additions.py` DEEP-MERGES them into the curated
> `Assets/Data/<type>/**` JSON as the **final offline step** (dicts recurse; leaves/lists override). It matches the
> curators' exact `indent=1` serialization, so an addition is a minimal additive diff, never a reformat.
>
> **The GAME never knows curation OR additions exist** (the c++ should not know or care that the json is
> now different from xml; the game does not, and should not know that there is such a thing as curation) — the
> whole Python pipeline (curators + additions re-apply) is a **separate offline entity** that merely PRODUCES the
> `Assets/Data` JSON the engine loads. The `_additions` files are the reviewable/revertible source layer.
>
> **⚖ THE RE-APPLY IS PART OF THE WRITE — there is no step to remember (it probably should be part of
> core loop).** A per-entity `--write` CLEARS its folder before rewriting, so running one curator alone used to
> silently drop that entity's overlay and leave the committed data disagreeing with a fresh regen. It was a
> documented instruction ("re-run `curate_additions.py` after any re-curate") and it was missed **more than once**
> — which is the point: a rule has to be remembered, a check does not ([AGENTS.md](../../../AGENTS.md)).
> So `curate_common` hooks the re-apply to the ONE act every writer must perform — clearing its folder
> (`wipe_entity_json`) — and runs it at process exit over exactly the folders that run rewrote. The merge stays
> the ONE implementation in `curate_additions`
> ([the DRY single-implementation law](../../architecture/patterns/03-dry-one-implementation-per.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)); the hook only decides
> WHEN it runs, and re-merging the same partial is a no-op, so `curate_all`'s closing pass still lands the same
> bytes.
> ⛔ **The hook reaches ONLY a writer that calls `wipe_entity_json` — a bespoke in-place write skips BOTH halves**
> (the drop-before-rewrite AND the overlay registration), so its regen silently sheds the type's overlays while
> looking complete. That is not hypothetical: `curate_unit` and `curate_unitcombat` each had exactly that write,
> and every unit regen dropped the settler's founder-culture overlay until they were routed through the wipe.
> **A curator's `--write` block calls `wipe_entity_json` on every folder it writes — no exceptions**; several fat
> curators still carry bespoke writes without it (inert only while their types have no `_additions` file).
> ⚠ A `--sample`/dry run clears nothing, so it registers nothing and applies no overlay — reading a sample still
> shows the pre-overlay curator output, which is what you want when checking the CURATOR.

## ⛔ THE CURATOR SKIPS DEAD THINGS — a MECHANISM, never a hand-kept list

An entity that produces **no effect, unlocks nothing, and is named by nothing** is dead weight: loaded resident,
listed in the manifest, offered in the build list and scored by the AI, all to do nothing. The legacy XML
accumulates these because a modder authors a shell and the field that would have made it work is dropped,
renamed, or never read — so the shell survives every later pass looking plausible. **Detecting them is the
curator's job, structurally** (`curate_common.skip_inert`), not a list somebody maintains by hand.

Two halves, both load-bearing:

- **INERT — FAIL-CLOSED, TWICE.** Only keys known to be effect-free (`cost`/`ui`/`world`/`sound`/`ai`, the
  constraints `requires`/`allowed`/`enabled`/`disabled`, and the target-side `obsoletedBy`/`replacedBy`) make an
  entity droppable. **A section the test has never heard of keeps the entity ALIVE**, so adding a
  [json.md](../json.md) section can never silently start deleting content — the worst it can do is decline a drop
  that would have been correct.
  ⛔ **`identity` is NOT blanket-inert, and assuming it is will delete live content.** [json.md §7](../json.md)
  is explicit that identity carries "intrinsic flags/values (radii, classifications, capability bools, base
  stats)" alongside the TEXT — so a plot feature is doing real work from inside identity via `movementCost` /
  `nukeImmune` / `noImprovement`, and across the curated set there are **213 distinct identity keys**, most of
  them behavioural. Only a small whitelist counts as inert (text · display/pedia placement · metadata ABOUT the
  entity such as `conquestProbability` / `mapCategories`); any other identity key makes the entity LIVE.
  ⚑ The whitelist is of **KEYS, not of dead entities** — that distinction is the point of the ruling. A key list
  describes the SCHEMA, so it is small, stable, and fails closed; a list of dead entities would be exactly the
  hand-maintained inventory this replaces.
- **UNREFERENCED — exhaustive, and it runs SECOND.** An entity can be inert and still load-bearing: a shell whose
  only job is to be another entity's prereq gate, an `obsoletes` target, an `enables` entry. Dropping one breaks
  the referrer, so the scan covers every XML record's every element plus Python and the DLL — over the handful
  the structural test already narrowed to, never over the whole category. ⚑ **This half is not belt-and-braces:**
  on its first run it held two of six building candidates that were genuinely pointed at.

**A drop ANNOUNCES, and so does a near-miss** — the [triggers.md](../triggers.md) census discipline: data that
vanishes and reports nothing is invisible on both axes at once. The kept-but-inert line is the more interesting
half, because it names an entity that does nothing on its own and exists only to be referenced.

⚠ **Distinct from `store.DROPPED_TYPES`**, which cuts a Type whole at the store because a whole SYSTEM was ruled
out. That is a DECISION; this is a DETECTION. Do not fold either into the other.

## Modules — the curator folds in the ones we want

The store enumerates `Assets/XML` **and** `Assets/Modules`, so a wanted module's records merge into the curated
output exactly like base XML. Unwanted modules are named in `store.py`'s `EXCLUDED_MODULE_SUBPATHS` and are never
ingested — **an intentional content boundary, not a coverage gap.** An excluded module's records in
still-XML-loaded categories (event triggers, spawns) can therefore name ids that no longer exist; that is expected
and is not fixed by re-admitting the module. Rationale + the known instances + the intended replacement:
[plans/parked/module-system-rework.md](../../plans/parked/module-system-rework.md).

## Contents
- **fixed-point-and-scales.md** — the ×100 fixed-point model AND the curator de-scale registry (which Info fields
  are ×100 vs ×1 — the closed set of `…100()` accessors + the blind-spot fields) in one file; it is the durable
  Home of [the ×100 fixed-point model](fixed-point-and-scales.md#1-the-model--integer-100-for-amounts-human-only-at-the-in-and-out-boundaries) / [the curator-owns-descale rule](fixed-point-and-scales.md#1-the-model--integer-100-for-amounts-human-only-at-the-in-and-out-boundaries) (carve-out above), not merely a registry alongside
  the model. [json.md §3.6](../json.md) is a separate, narrower thing — the `flat`/`percent`/`multiplier`
  magnitude-unit taxonomy a modifier value authors — and links back here for the scale rulings themselves.

The cascade ontology model (one cascade, every per-turn-effect producer is a target, sources/enablers are never
targets, the stay-vs-invert rule) is **durable** and lives in the specs — [modifier.md](../../cascade.md) (the
deliveryguy/inversion rules) and [enabler.md](../enabler.md) (the enabler topology). The per-entity curator
decisions live in each `curate_<entity>.py` docstring (the old→new map *is* the curator, see above).

## See also
- [../json.md](../json.md) — the durable JSON shape this produces. [../validation.md](../validation.md) — proves the
  produced data reaches parity. The curators themselves: `Tools/Migration/curate_*.py`.
