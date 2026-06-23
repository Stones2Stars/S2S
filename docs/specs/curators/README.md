# Curators — the migration conversion spec (transient)

> **Project-specific & temporary (owner ruling 2026-06-23).** The migration curators (`Tools/Migration/curate_*.py`)
> convert the legacy Civ4 XML into the clean JSON shapes the cascade reads. This area specs **what the curators do** —
> the per-entity conversion decisions and the de-scale registry. (The **old→new field map is NOT a doc** — it lives
> in the curators themselves; see below.)
>
> **Separate from [json.md](../json.md) on purpose:** json.md is the **durable** spec of the JSON *shape* (what the
> data IS); this area is **how the data got there** (transient). It is **dropped when the migration completes** — do
> NOT fold it into the durable JSON spec.
>
> *(These were lifted intact from the old migration set rather than condensed: they are transient working specs, so
> preserve-and-place beats a condensing investment. Their internal links still point at pre-move paths — part of the
> global reference sweep follow-up.)*

## The old→new map lives in the curators (owner ruling 2026-06-23)
There is **no rename-trail / infotype-translation doc** — the old→new field map **is** the curator code, and that is
where it stays (a doc copy poisons context and drifts). Each `curate_<entity>.py` **docstring annotates every new key
to its legacy field, with the why**, and the code right below implements it. Canonical exemplar —
`curate_gamespeed.py`:

> `speed.world.percent` = `iSpeedPercent` — the master game-pace percentage … · `missionYieldMultiplier.world.percent`
> = `iUnitYieldScalePercent` …

The mechanical de-Hungarianization (`iX` → `x`) lives in `engine.py`; the per-entity semantic renames live in each
curator's docstring + body. To read the map for an entity, **read its curator.**

## Contents
- **building-cascade-conversion.md** — the locked cascade ontology model + the per-entity curator decisions
  (stay-vs-invert, sources-never-targets, deferred edges, the post-migration purge backlog).
- **fixed-point-and-scales.md** — the curator de-scale registry: which Info fields are ×100 vs ×1 (the closed set of
  `…100()` accessors + the blind-spot fields). The fixed-point *model* lives in [json.md §3.6](../json.md).

## See also
- [../json.md](../json.md) — the durable JSON shape this produces. [../validation.md](../validation.md) — proves the
  produced data reaches parity. The curators themselves: `Tools/Migration/curate_*.py`.
