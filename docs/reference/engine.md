# Engine reference — the constraints the cascade runs on

> Lifted + condensed from the old `reference/engine/` set. The durable engine facts a fresh S2S engineer needs:
> the closed-`.exe` constraints, the systems the cascade reads or replaces, and the footguns. Behaviour **as it is
> today** — the cascade rework shadows each legacy maintainer ([logging](../specs/logging.md) §6) before cutting it.

## Toolchain — the locked closed-`.exe` stack

The closed Firaxis `.exe` (**VC7.1 / MSVC 7.1 / VC++ Toolkit 2003**) freezes the whole stack via ABI/STL sharing
across the process boundary — **not** style choices: **C++03, 32-bit, Python 2.4, Boost 1.32 + 1.55**.
- **Two Boosts coexist.** **1.32** (`boost::`) — general + the *only* compiled lib, `boost_python-vc71` (the
  C++↔Py2.4 bridge); can't be dropped (Boost.Python isn't header-only and no 1.55 Python lib can be built on this
  toolchain). **1.55** (`boost155::`, namespace-renamed, header-only) — used mainly via the `foreach_` /
  `reverse_foreach_` macros. 1.55 is the ceiling (post-1.55 Boost drops VC7.1).
- **PCH footgun:** never `using namespace boost*` — a bare `bind`/`function` can silently resolve to `boost::`
  through the PCH (bit `CvHttpServer`). The cascade event-spine deliberately names no Boost type.

## Save / load — name-keyed, soft for ADDS only

Format is **name-keyed, not positional** — `(id, type-code, value)` tuples; no save-version number; compatibility
resolves dynamically by name.
- **Adding** a field is SOFT (old save, new code): the new read mismatches the stream's next element, no-ops, and
  keeps its default — `Expect()` returns false and leaves the stream untouched
  (`Sources/Infrastructure/CvTaggedSaveFormatWrapper.cpp:3830`).
- **⛔ Removing a field's read is NOT soft — there is NO automatic drain of a stale tag (proven live 2026-07-02).**
  `Expect()` treats *any* mismatch as "the code is ahead of the stream" and never consumes the unexpected element,
  so an orphaned tag in an old save makes **every subsequent read in that object** mismatch against it and silently
  default — the load guts wholesale (the live failure: empty tech lists, buildingless cities). Retirement is
  **two-stage**: (a) delete the `WRAPPER_WRITE` and replace the read with a **named
  `WRAPPER_SKIP_ELEMENT(wrapper, "ClassName", memberName, SAVE_VALUE_ANY)`** — it Expects that exact tag and drains
  it on old saves, and safely no-ops on newer saves that lack it — and ledger the field in **`savemigration.txt`**
  (repo root, the conversion-step list); (b) flush the skips + ledger entries together at the next save-compat
  break.
- **Enum/Type drift** is name-remapped on load (`getInfoTypeForString`); XML reorder/insert is free. A removed Type
  is SOFT only if its read site uses `WRAPPER_READ_CLASS_ENUM_ALLOW_MISSING` (else HARD: message box + throw).
- **The genuinely HARD save-breaks:** (1) a same-tag field whose *meaning* changed (silent wrong load); (2) a
  deleted Type read without `_ALLOW_MISSING`; (3) a legacy raw enum-indexed int array that shrinks; (4) a type-code
  change under a reused name; (5) a deleted field read without its named `WRAPPER_SKIP_ELEMENT` (the stale-tag
  desync above). *Hardening path:* flip a Type's read site to `_ALLOW_MISSING`, then delete.
- **Derived data serializes nothing** — `reset()` + mark-dirty-on-load is the pattern; the cascade [tally](../specs/tally.md) never serializes.

## Pathfinding — two systems

- **`CvPathGenerator`** (`Sources/Infrastructure/`) is the **shipping unit pathfinder** (the legacy engine `FAStar`
  unit finder is compiled out). Fully pluggable via 5 typed callbacks (heuristic / cost / valid / terminus /
  turn-end); `generatePathForHypotheticalUnit` does distance probes with no live `CvUnit`. Shared via
  `CvSelectionGroup::getPathGenerator()`.
- **`FAStar`** still drives the **non-movement** queries: **step** (tile-hop distance), **route**, **border**,
  **area** (flood-fill), **plot-group** (trade net). Finders are **stateful + shared** — call `GetLastNode` on the
  *same* finder that ran `GeneratePath` (bug #73 read a stale global → wrong distance). `stepCost = 1` (a hop count,
  not turns or move-points). Quirk: `teamStepValid` checks diplomacy but **not** `isImpassable()` (team paths can
  cross impassable tiles). Finders are per-`MapTypes` (multimap).

## Properties — the generic attribute bag + its legacy auto-placement

- **`CvProperties`** is a generic `(PropertyTypes, int)` bag (values + per-turn rates) attachable to any object
  (game…plot). The mutating *rules* are **not** on it — they live in **`CvPropertyManipulators`** on info objects
  (buildings / handicaps / bonuses), run by the solver each turn.
- **`CvPropertySolver`** is a member of `CvGame` (**not** a singleton — `GC.getPropertySolver()` does NOT exist),
  run once per `doTurn` in fixed order **propagators → interactions → sources**, each a predict/compute/correct/apply
  pass (spread resolves against *pre-source* values, then production applies — counter-intuitive).
- **Band auto-placement** (the crime/disease/education/pollution buildings): `CvPropertyInfo` `PropertyBuilding`
  bands silently grant/revoke buildings as a value crosses thresholds (`checkPropertyBuildings`, skipped for NPCs).
  **A legacy maintainer the cascade replaces — shadow, then cut.** Property values fold into the OOS/save checksum.

## Map generation — Python callbacks, DLL fallback

- The DLL drives mapscripts via **named Python callbacks**; undefined ones fall back to DLL / `CvMapGeneratorUtil.py`
  defaults — the contract is the **callback names**, not the impl. (`generatePlotTypes` returning a list, an
  `addLakes` no-op, bare-`return` `normalize*` all suppress the DLL default.)
- **Footguns:** renaming a `TERRAIN_*` tag → the script's `getInfoTypeForString` returns −1 → engine crash later. MP
  determinism needs `PySeed()` in `beforeGeneration` (else clients diverge — Python `random` isn't seeded from
  `MapRand`). File split (non-obvious): `CvMapGenerator` is in `Sources/Infrastructure/`, `CvMap`/`CvGame` in `Sources/Engine/`.

## Gamespeed & calendar — all derived, no stored table

- Pacing lives in **`CvEraInfo`** (per-era historical start/end year, normal-speed turns) + **`CvGameSpeedInfo`**
  (speed %, unit-yield-scale %). **No stored turn→date table** — `CvDate::getDate(turn, speed)` interpolates over the
  era year-span / turn-count. The legacy `GameTurnInfos` tables, `iStartPercent`, and the separate historical-range
  defines are GONE; both calendars now share the `CvEraInfo` year fields.
- **`<Adapt>` XML tags** dispatch by tag name to a channel (`<Adapt>`→`getSpeedPercent`, `<AdaptHammerCost>`,
  `<AdaptUnitYield>`), single evaluator `CvGameObject::adaptValueToGame()`.

## Handicaps — two "handicaps", asymmetric

- **Per-player** (`m_aeHandicap`, saved) vs **game** (`m_eHandicap`, NOT saved — recomputed as the integer average
  of alive *human* players). **The asymmetry:** human-facing economic fields read the *owner's own* per-player
  handicap; **every `getAI*` advantage reads the GAME handicap** — so AI research/production/cost advantages scale
  with the *human's* difficulty, never the AI's own (all AIs default `NOBLE`). To make AIs economically stronger,
  raise the human difficulty. Sole exception: `getAIAdvancedStartPercent` reads the AI's own.
- Traps: score multiplies by the raw handicap **enum index** (reordering XML rows silently shifts score); barb spawn
  is **inverted** (a *lower* `getBarbarianCityCreationProb` = a *higher* spawn chance).

## Info loading — `CvInfoUtil` (current), superseded forward by `readJson`

- **`CvInfoUtil`** is the active XML loader: one `getDataMembers()` declaration derives
  read/copyNonDefaults/checkSum/init (`CvBuildInfo` is the reference). **But the forward direction is top-down JSON
  via `readJson` (#428/#430), which bypasses `CvInfoUtil` entirely** — do NOT chase the old "migrate remaining infos
  to declarative XML" goal.
- The asset **checksum** only triggers the modifier-recalc popup on mismatch — it does NOT gate MP OOS or block
  loading; full parity is not required when restructuring data (cost = one spurious popup per existing save).

## UnitCombat — the fat info class + the cascade-migration note

> Ties directly into the [unit-classification](../specs/skills.md) work — `tags` like `gunpowder`/`mounted` come
> from unitcombats (post-migration).

- Vanilla: a thin label. **S2S/C2C:** a fat `CvUnitCombatInfo` (~150 fields, near-mirror of `CvPromotionInfo` — a
  combat class ≈ a free promotion for every member), many-to-many membership, proliferated to **~981 classes (~77%
  attached to no unit — vestigial)**; ~96% of live classes are inert tags (size/species/motility taxonomies crammed
  into the combat-role enum).
- Combat resolution: **additive-accumulate, multiply-once** — ~40 signed-% layers sum into one `iModifier`, applied
  multiplicatively once; "vs X" folds into the *defender's* number. **Four overlapping "vs" channels** add into the
  same `iModifier` with no precedence (silent stacking; a known live bug swaps the vs-class / vs-unit help labels).
- **"Unreferenced ≠ dead"** — two purge blind spots: inactive-module classes that *look* orphaned, and engine
  attribute-matches (`doSetUnitCombats` selects by `getEra()`/`getReligion()`/`getCulture()`, never XML-named). A
  blunt 2026-06-14 purge over-reached on both and was fully reverted.
- **Cascade migration:** UnitCombat is a **source/enabler** (membership = the enabler axis; the ~150 fields = a
  modifier deposit) and should share Promotion's modifier-family vocabulary — **do UnitCombat + Promotion together**.
  Shadow-then-cut any dead-class purge.

## See also
- [../specs/](../specs/) — the cascade model the engine feeds. [../specs/logging.md](../specs/logging.md) — the
  shadow + map-before-delete bar that gates cutting any legacy maintainer above.
