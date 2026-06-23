# Curators — the migration conversion spec (transient)

> **Project-specific & temporary (owner ruling 2026-06-23).** The migration curators (`Tools/Migration/curate_*.py`)
> convert the legacy Civ4 XML into the clean JSON shapes the cascade reads. This area specs **what the curators do** —
> the per-entity conversion decisions, the de-scale registry, the JSON↔machine map, and the old→new rename trail.
>
> **Separate from [json.md](../json.md) on purpose:** json.md is the **durable** spec of the JSON *shape* (what the
> data IS); this area is **how the data got there** (transient). It is **dropped when the migration completes** — do
> NOT fold it into the durable JSON spec.
>
> *(These were lifted intact from the old migration set rather than condensed: they are transient working specs, so
> preserve-and-place beats a condensing investment. Their internal links still point at pre-move paths — part of the
> global reference sweep follow-up.)*

## Contents
- **building-cascade-conversion.md** — the locked cascade ontology model + the per-entity curator decisions
  (stay-vs-invert, sources-never-targets, deferred edges, the post-migration purge backlog; **CREST** is still open).
- **fixed-point-and-scales.md** — the curator de-scale registry: which Info fields are ×100 vs ×1 (the closed set of
  `…100()` accessors + the blind-spot fields). The fixed-point *model* lives in [json.md §3.6](../json.md).
- **infotype-translation.md** — the four-layer Rosetta: old XML ↔ legacy C++ (transient) and JSON key ↔ consuming
  cascade machine (cols 3+4, the durable map).
- **migration-renames.md** — the old→new field rename trail the `readJson` C++ pass needs.

## See also
- [../json.md](../json.md) — the durable JSON shape this produces. [../validation.md](../validation.md) — proves the
  produced data reaches parity. The curators themselves: `Tools/Migration/curate_*.py`.
