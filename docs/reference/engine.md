# Engine reference — the constraints the cascade runs on

> Lifted + condensed from the old `reference/engine/` set. The durable engine facts a fresh S2S engineer needs:
> the closed-`.exe` constraints, the systems the cascade reads or replaces, and the footguns. Behaviour **as it is
> today** — the cascade rework replaces each legacy maintainer ([logging](../specs/logging.md) §6), verified live before cutting it.

## Toolchain — the locked closed-`.exe` stack

The closed Firaxis `.exe` (**VC7.1 / MSVC 7.1 / VC++ Toolkit 2003**) freezes the whole stack via ABI/STL sharing
across the process boundary — **not** style choices: **C++03, 32-bit, Python 2.4, Boost 1.32 + 1.55**.

- **Two Boosts coexist.** **1.32** (`boost::`) — general + the *only* compiled lib, `boost_python-vc71` (the
  C++↔Py2.4 bridge); can't be dropped (Boost.Python isn't header-only and no 1.55 Python lib can be built on this
  toolchain). **1.55** (`boost155::`, namespace-renamed, header-only) — used mainly via the `foreach_` /
  `reverse_foreach_` macros. 1.55 is the ceiling (post-1.55 Boost drops VC7.1).
- **PCH footgun:** never `using namespace boost*` — a bare `bind`/`function` can silently resolve to `boost::`
  through the PCH (bit `CvHttpServer`). The cascade event-spine deliberately names no Boost type.

## Save / load

The name-keyed save format, the soft-add / soft-remove rules, the `Assets/savemigration.txt` drain, the two kinds of
`WRAPPER_SKIP_ELEMENT`, derived-serializes-nothing, and the changer-body side-effect audit now live in their own core
spec — **[../specs/save.md](../specs/save.md)** (home of [DEC-save-remove-is-soft](../architecture/decisions.md#dec-save-remove-is-soft)
+ [DEC-derived-never-trusted](../architecture/decisions.md#dec-derived-never-trusted)). The one-line reminders that
matter for engine work: field removal is a soft `savemigration.txt` drain (**never** a `WRAPPER_SKIP_ELEMENT`, never a
save-break); derived data serializes nothing; deleting a changer means auditing its whole body for riders.

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

> **⚖ The property engine is SELF-CONTAINED BY DESIGN** — what happens inside the property engine stays inside
> the property engine. Its internal semantics (e.g. the cascade property channel's own per-handling,
> `CvCascadeProperty`) are not unified with the generic modifier machinery, and the generic per-count resolver
> serves the ordinary modifier channels, never threads into the property engine.

- **`CvProperties`** is a generic `(PropertyTypes, int)` bag (values + per-turn rates) attachable to any object
  (game…plot). The mutating *rules* are **not** on it — they live in **`CvPropertyManipulators`** on info objects
  (buildings / handicaps / bonuses), run by the solver each turn.
- **`CvPropertySolver`** is a member of `CvGame` (**not** a singleton — `GC.getPropertySolver()` does NOT exist),
  run once per `doTurn` in fixed order **propagators → interactions → sources**, each a predict/compute/correct/apply
  pass (spread resolves against *pre-source* values, then production applies — counter-intuitive).
- **Band auto-placement** (the crime/disease/education/pollution buildings): `CvPropertyInfo` `PropertyBuilding`
  bands silently grant/revoke buildings as a value crosses thresholds (`checkPropertyBuildings`, skipped for NPCs).
  **A legacy maintainer the cascade replaces — verified live, then cut.** Property values fold into the OOS/save checksum.

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
- **Category id ORDER comes from the `_order.json` manifest** (`Assets/Data/<cat>/_order.json`, curator-derived —
  `Tools/Migration/curate_order.py`): the loader sorts a category's entities by manifest position before
  registering ids, so the engine ids reproduce the LEGACY id order (base XML document order, then module
  additions) and every id-ordered UI surface keeps its familiar layout (the level-up promotion popup groups each
  line's tiers adjacently because the XML did). A type absent from the manifest (synthetic `TECH_GAME_START`,
  future additions, a manifest-less category) sorts AFTER every listed one, alphabetically — the legacy
  new-stuff-appends-last behaviour. Manifests are derived artifacts: regenerate + commit freely, never hand-edit.
- **`LoadGlobalClassInfoJson` is TWO-PASS by requirement, never one.** Each JSON category loads by (1) registering
  **every** type→id (`setInfoTypeFromString`), then (2) running `mapFrom` on each entity. `mapFrom` resolves its FKs
  at parse time (`jsonResolveId` → `getInfoTypeForString(id, /*bHideAssert*/true)`), so a single register-then-map
  pass silently DROPS any **same-category forward reference** — an id naming a sibling that sorts *after* its owner
  (entities load sorted by type name). The miss is invisible (`bHideAssert` writes no `Xml_MissingTypes.log` line);
  it only shows in the cascade FK census (`jsonUnresolvedIds`, capped at 64). This severed ~47% of unit
  `requires.build.dormant`/`replacedBy.units` edges — the entire upgrade/dormancy chain
  (machete→musketman→rifleman→trench_infantry, every trigger sorted after its owner), so no old unit went
  dormant/replaced and the build list showed everything. The two-pass load is the fix; **do NOT collapse it back**.
  Cross-category forward refs (an earlier-loaded category naming a later one — specialist→UNIT, building/unit→
  CIVILIZATION, …) are resolved by the SAME principle one level up: `cascadeLoadJson`'s full-registry pass re-runs
  the complete `mapFrom` on every aliased entity once ALL categories are registered — `mapFrom` is idempotent by
  contract (`CvInfo.h`), and `/state/info?type=X` is the standing loaded≡authored verification
  verification.

## UnitCombat — the fat info class + the cascade-migration note

> Ties directly into the [unit-classification](../specs/skills.md) work — `tags` like `gunpowder`/`mounted` come
> from unitcombats (post-migration).

- **What a UnitCombat IS (owner):** a definition of a unit's **strengths and weaknesses** — the good/bad-against
  column (a shared vs-tag stat bundle), NOT a definition of the unit's TYPE (that is the [tag](../specs/skills.md))
  nor its ABILITIES (those are skills). Three concerns, three homes. This is what it originally was in BTS (a
  vs-based combat grouping); the S2S distillation restores it ([unitcombat-distillation.md](../plans/structural-cleanup/unitcombat-distillation.md)).
- Vanilla: a thin label. **S2S/C2C:** a fat `CvUnitCombatInfo` (~150 fields, near-mirror of `CvPromotionInfo` — a
  combat class ≈ a free promotion for every member), many-to-many membership, proliferated to **~981 classes (~77%
  attached to no unit — vestigial)**; ~96% of live classes are inert tags (size/species/motility taxonomies crammed
  into the combat-role enum). **The proliferation came largely from the killed EQUIPMENT mod (owner)** — it minted a
  combat class per equipment permutation, which is why the enum bloated into a size/species/weapon taxonomy far
  beyond the strengths/weaknesses role.
- Combat resolution: **additive-accumulate, multiply-once** — ~40 signed-% layers sum into one `iModifier`, applied
  multiplicatively once; "vs X" folds into the *defender's* number. **Four overlapping "vs" channels** add into the
  same `iModifier` with no precedence (silent stacking; a known live bug swaps the vs-class / vs-unit help labels).
- **"Unreferenced ≠ dead"** — two purge blind spots: inactive-module classes that *look* orphaned (Cultures /
  Alt_Timelines / Ideas / ExoticAnimals module XML holds the assignments), and engine runtime-attachment. The ONLY
  runtime-attach selector is `getEra()`: `doSetUnitCombats` (`CvUnit.cpp:26140`) attaches the first combat class whose
  `getEra()` matches the unit's era, on top of the unit's primary/sub `combatClass`es, promotion grants, and heal-as
  types. `identity.religion` is read FROM already-attached combats (`CvUnit::getReligion`, `:30868`), NOT an attach
  selector; `identity.culture` has no attach path at all. A blunt 2026-06-14 purge over-reached on the module blind
  spot and was fully reverted.
- **Cascade migration:** a UnitCombat is a modifier SOURCE — its vs-tag stats deposit onto the units that carry it,
  sharing Promotion's modifier-family vocabulary (**do UnitCombat + Promotion together**). Its non-stat content
  distills out: identity → tags, abilities → skills, leaving the pure strengths/weaknesses list. Verify live, then
  purge only vestigial/duplicate classes.

## See also

- [../specs/](../specs/) — the cascade model the engine feeds. [../specs/logging.md](../specs/logging.md) — the
  map-before-delete + observability bar that gates cutting any legacy maintainer above.
