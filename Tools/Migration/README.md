# `Tools/Migration/` — the #428 XML→JSON curation toolkit

Offline Python that converts the legacy `Assets/XML/` gameplay data into the top-down
cascade JSON under **`Assets/Data/`**. The JSON is the migration target for #428/#430
(see the spec cluster in `docs/specs/`: `json`, `enabler`, `modifier`, `tally`,
`naming`). This README is the **operational runner reference** — *which script
to run, what it writes, and the footguns*. The model/shape lives in the specs above.

---

## TL;DR — how to re-export an entity

The authoritative pipeline is **one hand-written curator per entity**:

```bash
python Tools/Migration/curate_<entity>.py --sample <TYPE>   # print converted record(s), write NOTHING
python Tools/Migration/curate_<entity>.py --write           # regenerate that entity's whole folder
```

> **⛔ RULE — edit a curator, regenerate its data in the SAME change (owner ruling 2026-06-21).** A modified
> `curate_<entity>.py` whose `Assets/Data/<entity>/` has NOT been re-`--write`n is a **curator/data mismatch**:
> the curator claims one shape, the data on disk is another, and every downstream consumer (the dry-calc, the
> shadow, the DLL reader) silently trusts the stale data. Always run `--write` immediately after touching a
> curator and commit the regenerated JSON alongside it. Never leave a curator edit uncommitted-and-unregenerated.

`--write` regenerates the **entire** entity (e.g. `curate_unit.py --write` rewrites all
~2073 unit files under `Assets/Data/units/`). Output goes to `Assets/Data/<entity>/`
(notable layouts: **units** → `units/` + `specialunits/`; **buildings** → `buildings/<era>/`).

**The workflow when a curator is wrong** (we misunderstood the XML, per the owner rule
"if the data is wrong because we misunderstood the XML, we re-export the data"):
1. Fix the curator (`curate_<entity>.py`, or `store.py` for cross-entity edges).
2. Re-export **only that entity**: `python Tools/Migration/curate_<entity>.py --write`.
3. Spot-check the JSON (`--sample`, or read a representative file); the owner visually
   inspects + commits. JSON is **generated, never hand-edited** — a hand edit is lost on
   the next `--write`.

---

## ⛔ FOOTGUNS — read before running anything

- **Don't run `engine.py --write` for the curated DB — use the per-curator `curate_*.py`.**
  `engine.py` has a **dual role**: (a) the **shared-helper module** every curator does
  `import engine` for (`FIELD_RENAME`, text/generic helpers — alive and essential) AND (b) a
  **superseded standalone mapping-driven emitter** whose `--write` rewrites the *whole*
  `Assets/Data/` with the **OLD raw shapes** (`cost:{iCost:…}`, `prerequisites`, flat layout) —
  not the curated `requires` shapes — so it produces the wrong (superseded) output over ~9500
  files. **Not a catastrophe, though: regen is idempotent and takes seconds** — if you run it by
  mistake just re-run the curators (or `git checkout -- Assets/Data`). The curated data is
  produced by the `curate_*.py` curators, **not** by `engine.py`.
- **`migrate_buildings.py` is SUPERSEDED** — the "first cut" whole-entity building converter,
  replaced by `curate_building.py`. Don't run it to (re)generate buildings; use
  `curate_building.py --write`.
- A curator's `--write` **overwrites its whole entity folder.** That's intended (the JSON is
  generated), but it means any uncommitted hand-edit to that entity's JSON is gone. Don't
  hand-edit `Assets/Data/`; fix the curator.

---

## The pieces

### Authoritative — the per-entity curators (run these)
`curate_<entity>.py --write` — one bespoke curator per entity, each `import`ing the shared
core. Coverage (run the matching script to re-export):

| entity | script | entity | script |
|---|---|---|---|
| Bonus | `curate_bonus.py` | Improvement | `curate_improvement.py` |
| BonusClass | `curate_bonusclass.py` | LeaderHead | `curate_leaderhead.py` |
| Build | `curate_build.py` | Process | `curate_process.py` |
| Building (+SpecialBuilding) | `curate_building.py` | Project | `curate_project.py` |
| Civic | `curate_civic.py` | Promotion | `curate_promotion.py` |
| CivicOption | `curate_civicoption.py` | PromotionLine | `curate_promotionline.py` |
| Civilization | `curate_civilization.py` | Property | `curate_property.py` |
| Corporation | `curate_corporation.py` | Religion | `curate_religion.py` |
| CultureLevel | `curate_culturelevel.py` | Route | `curate_route.py` |
| Era | `curate_era.py` | Specialist | `curate_specialist.py` |
| Feature | `curate_feature.py` | Tech | `curate_tech.py` |
| GameSpeed | `curate_gamespeed.py` | Terrain | `curate_terrain.py` |
| Handicap | `curate_handicap.py` | Trait | `curate_trait.py` |
| Heritage | `curate_heritage.py` | Unit (+SpecialUnit) | `curate_unit.py` |
| Hurry | `curate_hurry.py` | UnitCombat | `curate_unitcombat.py` |
| | | Victory | `curate_victory.py` |
| | | Vote | `curate_vote.py` |

`curate_pocos.py` — batch curator for verified data-holder entities (text/identity only).
Note the owner ruling "**no entity is truly a POCO**": a 0-channel classification is a
hypothesis to disprove against the live C++ consumer, not a fast path.

### Shared modules (imported, NOT run standalone for the curated pipeline)
- **`store.py`** — XML-as-DB. Loads every gameplay Info from base XML + `Assets/Modules`
  (merged by Type), and builds the generic **enable/obsolete reverse index** by inverting
  prereq fields (`PREREQ_FIELDS`/`OBSOLETE_FIELDS`). The curators read `Store` for the
  forward `enables` edges. Has a query CLI only: `python store.py --enables TECH_X` (print
  what a Type enables); it does **not** write JSON. Register a new source entity in
  `store.ENTITIES` (+ its prereq/obsolete edges) before curating it.
- **`curate_common.py`** — the shared core: `curate()` (assembles type/text/`enables`/
  `obsoletes`/modifier-families/`grants`/`ai`/art/`identity`), `EntityConfig` (driven by
  `mapping/<Entity>.json` + per-field `families` overrides), the `ART_BLOCK` map. Thin
  entities (tech/bonus/process/route/…) ride this via a tiny config; bespoke curators
  (building/unit/civic/trait/…) own their field tables but still call into it.
- **`engine.py`** — **shared helpers** (`FIELD_RENAME`, text/generic/`formula_node`/plural,
  the yield/commerce key tables). Imported by `curate_common` + most curators. ⛔ Its
  `--write`/`--dry` standalone mode is the **superseded raw emitter** — see FOOTGUNS.
- **`boolexpr.py`** — shared `BoolExpr → requires-condition` converter (the XML `And`/`Or`/
  `Has`/`Is` machinery → `all`/`any`/`noneOf` + predicates). Used to fold building
  `ConstructCondition`/`NewCityFree` and unit `TrainCondition` into `requires`. Raises on any
  unknown node/GOM/tag (so a new module construct is caught, never silently mis-converted).

### Inputs / scratch
- **`mapping/<Entity>.json`** — first-pass field classification (the 2026-06-13 workflow).
  **A FILTER, not gospel** — it under-classifies real gameplay; always verify a curator's
  output against the live C++ consumer.
- **`classifications/`** — saved adversarial-workflow per-field dispositions (Era + the light
  batch); curate from these rather than re-running the token-heavy workflows.
- **`extract_tags.py`** — per-entity distinct-tag extractor (analysis helper; no write).

---

## Dependency sketch

```
Assets/XML/  ──►  store.py (XML-as-DB + inversion index)
                      │
       engine.py (helpers) ─┐
                            ▼
                   curate_common.py (curate(), EntityConfig)
                            ▼
            curate_<entity>.py  ──►  Assets/Data/<entity>/*.json   (run with --write)
```

`engine.py --write` and `migrate_buildings.py` sit **outside** this path (superseded
emitters) — do not run them.

---

## Atom shape quick-reference (what a curator emits into `requires`)

A `requires` leaf atom is `{ "type": "PREFIX_NAME", "scope": "...", "min"?, "max"?,
"connection"? }` (data-model-spec §2.4). The `_atom(typ, scope, **kw)` helper builds it.
Scopes: `world`/`team`/`empire`/`area`/`city`/`plot`. Examples a curator produces:
- resource — `{"type":"BONUS_IRON","scope":"city","connection":"trade|vicinity"}`
- in-city building — `{"type":"BUILDING_FORGE","scope":"city"}`
- **tech (per-candidate CONFIRM, AND-only for units/buildings)** —
  `{"type":"TECH_ROBOTICS","scope":"team"}` in `requires.build.all`. The forward `enables`
  edge (on the tech) drives generation; the `requires.build` tech atom confirms the multi-tech
  AND (enabler-spec §3 / §13.8 — "the rare `TechTypes` multi-tech building/unit rides the same
  fix"). Only **techs themselves** carry OR-techs (`requires.build.any`); units/buildings are
  AND-only.
