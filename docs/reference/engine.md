# Engine reference — the constraints the cascade runs on

> Lifted + condensed from the old `reference/engine/` set. The durable engine facts a fresh S2S engineer needs:
> the closed-`.exe` constraints, the systems the cascade reads or replaces, and the footguns. Behaviour **as it is
> today** — the cascade rework replaces each legacy maintainer ([spine.md § What to log](../spine.md)), verified live before cutting it.

## Toolchain — the locked closed-`.exe` stack

The closed Firaxis `.exe` (**VC7.1 / MSVC 7.1 / VC++ Toolkit 2003**) freezes the whole stack via ABI/STL sharing
across the process boundary — **not** style choices: **C++03, 32-bit, Python 2.4, Boost 1.32 + 1.55**.

- **Two Boosts coexist.** **1.32** (`boost::`) — general + the *only* compiled lib, `boost_python-vc71` (the
  C++↔Py2.4 bridge); can't be dropped (Boost.Python isn't header-only and no 1.55 Python lib can be built on this
  toolchain). **1.55** (`boost155::`, namespace-renamed, header-only) — used mainly via the `foreach_` /
  `reverse_foreach_` macros. 1.55 is the ceiling (post-1.55 Boost drops VC7.1).
- **PCH footgun:** never `using namespace boost*` — a bare `bind`/`function` can silently resolve to `boost::`
  through the PCH (bit `CvHttpServer`). The cascade event-spine deliberately names no Boost type.

## Is a symbol really EXE-bound? — the decisive test

"`DllExport` because the closed EXE calls it" is the standing justification for keeping a legacy name, and it is
**checkable**, so it is never taken on trust.

⛔ **An import-table check answers NOTHING here.** `Civ4BeyondSword.exe` has **no static import entry for the
game-core DLL** — it loads it dynamically. Concluding "the EXE does not import it, so it is free" from the import
table is therefore a false negative for every symbol.

**The decisive test:** the EXE resolves the DLL's functions at runtime **by mangled name**, so its lookup keys are
present in the binary as plain strings. Parse the DLL's export directory for the mangled names, then test each one
for literal presence in the EXE image:

- present  ⇒ the EXE resolves that symbol ⇒ **a real ABI obligation**: the name, signature and calling convention
  are fixed and the symbol cannot be renamed or removed.
- absent   ⇒ **no ABI obligation** ⇒ it is ordinary DLL-internal surface and may be renamed, re-homed, or deleted
  like anything else.

Measured against the deployed `Assets/CvGameCoreDLL.dll`: **1,205 of 1,302 exports are EXE-referenced; 97 are
not.** The 97 are the ones a cut may freely take. ⚠ The test needs a DEPLOYED DLL to read the export table from,
so run it against the last good build, not a red tree.

## ⛔ WHAT WE HAND THE EXE ACROSS THAT BOUNDARY — the string contract

A symbol being EXE-bound fixes its NAME and SIGNATURE. It also fixes something the signature does not say: **what
the EXE is willing to receive.** The EXE never validates our return values — it walks them straight into
`strlen`/`wcslen`/`assign`, with no null check and no length of its own. Two rules follow, and each was paid for.

**⛔ A `NULL` RETURN CAN BE LOAD-BEARING — DO NOT "TIDY" ONE INTO AN EMPTY STRING.** For a *display* string, empty
and absent mean the same thing. For a **KEY** they do not: `NULL` says *there is no key, skip the lookup*, while
`L""` says *look up the empty key*, which sends the EXE down a path it was skipping. ⚑ Measured: changing
`CvActionInfo::getTextKeyWide`'s no-hotkey fallback from `NULL` to `L""` — purely for symmetry with the four
sibling getters beside it, which legitimately return `L""` because they return TEXT — **crashed the game on load**,
in the EXE's own `basic_string::assign` → `_Grow` → `memcpy`, with no DLL frame on the stack. Our own code treats
the null key as an expected state (`CvInfoBase.cpp`'s `getTextKeyWide() == NULL` test), which is the tell that was
there to read. ⇒ The siblings' convention is not evidence about a key; ask what the value IS, not what its
neighbours return.

**⛔ A POINTER WE RETURN MUST OUTLIVE THE CALL — THE EXE KEEPS IT.** Anything handed out as `const wchar_t*` /
`const char*` must stay valid for the life of the object, because the EXE caches it and re-walks it later. ⚑ The
worked case is `CvInfoBase::getDescription`, which returns a pointer INTO an element of its own translated-text
cache: while that cache was a `std::vector`, the next `push_back` reallocated and moved every element, so every
pointer issued earlier pointed at freed heap. It is a `std::deque` for exactly this reason — `push_back` never
invalidates references to existing elements (C++03 23.2.1.3). ⚠ The signature is `wcslen` faulting on a garbage,
NON-ZERO, VARYING address with the EXE as the only caller on the stack; a fault at address 0 is the null case
above instead, and they are different bugs.
⚑ **Both are diagnosable the same way:** the caught-exception dump (`CaughtDump-*.dmp`,
[external-tools-and-workflows.md](external-tools-and-workflows.md)) names the faulting CRT routine and the bad
address, which is what separates "we returned nothing" from "we returned something that has since died".

## Save / load

The name-keyed save format, the soft-add / soft-remove rules, the `Assets/savemigration.txt` drain, the two kinds of
`WRAPPER_SKIP_ELEMENT`, derived-serializes-nothing, and the changer-body side-effect audit now live in their own core
spec — **[../specs/save.md](../specs/save.md)** (home of [the soft-remove save discipline](../specs/save.md#3-removing-a-serialized-field--the-soft-remove-via-assetssavemigrationtxt-)
+ [derived data is never trusted from a save](../specs/save.md#5-derived-data-serializes-nothing-)). The one-line reminders that
matter for engine work: field removal is a soft `savemigration.txt` drain (**never** a `WRAPPER_SKIP_ELEMENT`, never a
save-break); derived data serializes nothing; deleting a changer means auditing its whole body for riders.

## ⛔ NO FLOAT WHERE IT CAN REACH SYNCHRONIZED STATE (owner)

**Home of [no float where it can reach synchronized state](#-no-float-where-it-can-reach-synchronized-state-owner).**

> *"Using float in any calc that is used in any kind of multiplayer scenario sounds like a gigantic no."*

Civ4 multiplayer is deterministic lockstep: every client runs the same turn and must reach the same state.
CPU-dependent float math (`pow`, `exp`, x87-vs-SSE intermediates, compiler reassociation) can differ in the last
bits, and a **truncation to int turns that into a different answer** — which is an OOS, not a rounding wobble.

**⚖ THE DISCRIMINATOR IS SYNCHRONIZED STATE, NOT "GAMEPLAY" (owner): *"gameplay path does not always mean
multiplayer."*** The test is whether the value can reach state every client must agree on:

- **BANNED** — anything feeding a STATE MUTATION or a DECISION every client computes. **An AI decision counts**:
  the AI runs on all clients, so a divergent score picks a different target on one of them.
- **FINE** — anything that dies at the screen: symbol offsets, animation times, health-bar widths, map pixel
  dimensions, and the `*Float` combat-strength reads behind the odds display. ⚠ Display float is already ruled a
  non-OOS ([patterns.md](../architecture/patterns.md) § the DLL does not convert for display) — that is a
  statement about *where the value ends*, never a licence to compute gameplay in float and print it.

⚑ **THE CONVERSION SHAPE, worked on `applyDistanceScoringFactor`** (an AI target-attractiveness decay that ran
`pow`/`exp` per call): a curve that factorizes into terms each depending on ONE input becomes **compile-time
integer tables in ×10000 fixed point**, multiplied in `int64_t` and reduced once. Two lookups and a multiply
replace two transcendental calls, and the result is bit-identical on every CPU.
⚠ **Acceptance is the ORDERING, not bit-equality with the float version** — the float answer was never
well-defined across clients, so it is not the baseline. Measured there: 87% of cases identical, worst absolute
difference 66 on a score of 1,000,000, and every ranking change confined to candidates whose scores differed by
≤ 1. A near-tie resolving differently is not a behaviour change; it is the tie being resolved *reproducibly*.

## ⛔ THE SYNCHRONIZED RNG IS SHARED SAVE STATE — do not touch the draws (owner)

**Home of [the synchronized RNG is shared state](#-the-synchronized-rng-is-shared-save-state--do-not-touch-the-draws-owner).**

There are three random streams, and only the distinction between them is what keeps a game in sync:

| stream | where | serialized? | what it is for |
|---|---|---|---|
| **`CvGame::getSorenRand`** (`getSorenRandNum`, `CvGame::SorenRand`) | `CvGame::m_sorenRand` | **YES** — `CvGame::read`/`write` | **every gameplay decision.** The overwhelming majority of draws in the tree. |
| `CvGame::getMapRand` | `CvGame::m_mapRand` | **YES** | world generation |
| `GC.getASyncRand()` | `CvGlobals`, not `CvGame` | **NO** | UI / cosmetic only — never a gameplay outcome |

⛔ **The synchronized stream's seed rides the SAVE, and every client advances it in lockstep. So the NUMBER of
values drawn, their ORDER, and whether a draw happens at all are shared game state — not implementation detail.**
Adding, removing, reordering or short-circuiting a `getSorenRandNum` call changes the sequence every other client
and every later turn sees: the symptom is an out-of-sync in multiplayer and a save that no longer replays.
⚑ This is why *"it draws from `SorenRand`"* is a **live named reason** to leave a body's shape alone — one of the
few that [superseded-ideas #22](../architecture/superseded-ideas.md) accepts in place of the dead
"mirror the legacy behaviour" argument. It is a statement about shared state, never nostalgia for legacy code.
⚠ Beware the subtle form: a short-circuit (`bCheap && getSorenRandNum(...)`) skips the draw when the left side is
false, so a refactor that merely REORDERS a condition can desync the stream without touching a single draw.

**⛔ THE RNG IS NOT DATA, AND NO JSON AUTHORS IT (owner).** No seed, stream, or draw is curated, and neither the
cascade nor the curator owns any part of it — do not model it, do not migrate it, do not invent a `random`
vocabulary. ⚑ **The line to hold, because it is easy to blur:** what JSON authors is the **ODDS** — a plain number
(`chance`, a probability percent) that the engine's own roll compares against. The **ROLL** is engine mechanism on
the synchronized stream. Authoring the threshold is data; performing the draw is not.

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
  (buildings / handicaps / bonuses), run by the solver each turn. Every value change announces
  `SEVT_PROPERTY_ADDED / _REMOVED` from the bag's own mutation sites (+ the in-read reseed) —
  [spine.md](../spine.md). ⚠ The same class doubles as authored INFO data (`CvOutcome`,
  `CvEventInfo`, `CvEventTriggerInfo` prereqs); those instances are default-constructed with a NULL game object,
  which is exactly what keeps a data parse silent on both the notification hook and the spine.
- **`CvPropertySolver`** is a member of `CvGame` (**not** a singleton — `GC.getPropertySolver()` does NOT exist),
  run once per `doTurn` in fixed order **propagators → interactions → sources**, each a predict/compute/correct/apply
  pass (spread resolves against *pre-source* values, then production applies — counter-intuitive).
- **Band auto-placement** (the crime/disease/education/pollution buildings): legacy `CvPropertyInfo`
  `PropertyBuilding` bands silently granted/revoked buildings as a value crossed a threshold, re-derived per turn
  and skipped for NPCs. **That per-turn maintainer is CUT** — the band is now a `requires.operate` PROPERTY clause
  the enabler holds, and the building itself is placed once by `CvCity::placeSystemBuildings` (see
  [../specs/enabler.md](../specs/enabler.md)). Property values fold into the OOS/save checksum.

## Map generation — Python callbacks, DLL fallback

- The DLL drives mapscripts via **named Python callbacks**; undefined ones fall back to DLL / `CvMapGeneratorUtil.py`
  defaults — the contract is the **callback names**, not the impl. (`generatePlotTypes` returning a list, an
  `addLakes` no-op, bare-`return` `normalize*` all suppress the DLL default.)
- **Footguns:** renaming a `TERRAIN_*` tag → the script's `getInfoTypeForString` returns −1 → engine crash later. MP
  determinism needs `PySeed()` in `beforeGeneration` (else clients diverge — Python `random` isn't seeded from
  `MapRand`). File split (non-obvious): `CvMapGenerator` is in `Sources/Infrastructure/`, `CvMap`/`CvGame` in `Sources/Engine/`.

## Gamespeed & calendar — all derived, no stored table

- Pacing lives in **`CvEraInfo`** (per-era historical start/end year, normal-speed turns) + **`CvGameSpeedInfo`**
  (speed %, unit-yield-scale % — served as `getScalar` reads on the exemplar surface; no `100` suffix per the
  scale-naming ruling, [fixed-point-and-scales.md](../specs/curators/fixed-point-and-scales.md)). **No stored turn→date
  table** — `CvDate::getDate(turn, speed)` interpolates over the era year-span / turn-count. The legacy
  `GameTurnInfos` tables, `iStartPercent`, and the separate historical-range defines are GONE; both calendars now
  share the `CvEraInfo` year fields.
- **`<Adapt>` XML tags** dispatch by tag name to a channel (`<Adapt>`→the speed scalar, `<AdaptHammerCost>`,
  `<AdaptUnitYield>`), single evaluator `CvGameObject::adaptValueToGame()`. The option-composed hammer-cost
  derivation is a consuming-system calc (json.md §9 — never an info getter).
- **`CvGameSpeedScale` (`Sources/Engine/`) is the ONE consuming-system calc for "scale this by game speed"** —
  `speedPercent()` / `hammerCostPercent()` / `missionYieldPercent()`, each returning a HUMAN percent
  ([the DRY single-implementation law](../architecture/patterns.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)). It exists because the
  info deliberately cannot serve two of them: `hammerCostPercent` composes `GAMEOPTION_EXP_UPSCALED_BUILDING_AND_UNIT_COSTS`
  with `UPSCALED_HAMMER_COST_MODIFIER`, and **an info never reads game state** (json.md §9 — a game option gates
  at the CONSUMING system). ⚠ It converts NOTHING: `CvGameSpeedInfo` serves `speed.world.percent` /
  `missionYieldMultiplier.world.percent` as straggler scalars (`getScalar(SCALAR_SPEED, CASC_SCOPE_WORLD,
  CASC_UNIT_PERCENT)`), and a **percent is not scaled**
  ([the ×100 fixed-point model](../specs/curators/fixed-point-and-scales.md#1-the-model--integer-100-for-amounts-human-only-at-the-in-and-out-boundaries)), so the value is already what every
  caller wants. ⛔ Do not re-derive either percent at a call site, and do not add a `/100` to "correct" it.

## Consuming-system calcs — where an option-composed verdict lives

**⚖ An info never reads game state, so a value composing a GAME OPTION gets its own consuming-system calc — one
place, never re-derived per call site** (json.md §9 +
[the DRY single-implementation law](../architecture/patterns.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)). This generalizes the
`CvGameSpeedScale` note above, and is the shape any future one copies: a purely-organizational static-methods
class (no data members, never instantiated — a namespace risks VC7.1/Boost name-mangling) holding the
composition the info structurally cannot.

| calc | composes | replaces |
|---|---|---|
| **`CvGameSpeedScale`** (`Sources/Engine/`) | `GAMEOPTION_EXP_UPSCALED_BUILDING_AND_UNIT_COSTS` × `UPSCALED_HAMMER_COST_MODIFIER` over the gamespeed scalars | per-call-site re-derivation of the hammer-cost percent |
| **`CvTraitSelection`** (`Sources/Engine/`) | `GAMEOPTION_LEADER_NO_NEGATIVE_TRAITS` / `START_NO_POSITIVE_TRAITS` / `LEADER_DEVELOPING` against a trait's alignment + succession rank | the archived `CvTraitInfo::isValidTrait` (an info getter reading game state — the boundary violation itself), **and** the hand-inlined copies of its composition that had spread across `CvPlayer` and `CvGameTextMgr` |
| **`CvBuildCostScale`** (`Sources/Engine/`) | the two BUILD-cost option verdicts: a building's authored cost BANDS under `GAMEOPTION_REALISTIC_BUILDING_COST`, and a unit's training PACE (`UNIT_PRODUCTION_PERCENT` vs `…_SM`) under `GAMEOPTION_COMBAT_SIZE_MATTERS` | the inline derivations in `CvPlayer::getProductionNeeded(BuildingTypes)` / `getBaseUnitCost` — the building half additionally carried a `float` multiplier on a path every build decision runs (an OOS hazard; the bands are whole percent points, so the integer form is exact) |

⚑ **The tell that one is needed: the same option composition appearing at more than one call site.** Those copies
DRIFT — the `CvGameTextMgr` inlines had already lost two legs of the rule (the negative-trait
`START_NO_POSITIVE × DEVELOPING` clause, and the barbarian-selection carve-out), so the set of traits the UI
showed as selectable disagreed with the set the engine allowed. Consolidating adopts the fuller rule.
⚠ **Not every use of these options is that verdict:** the leader level-up valuation reads
`START_NO_POSITIVE_TRAITS` as a level/weighting modifier, which is a different question and correctly stays
where it is. Read what the option is being ASKED, never match on the option name.

## Handicaps — two "handicaps", asymmetric

- **Per-player** (`m_aeHandicap`, saved) vs **game** (`m_eHandicap`, NOT saved — recomputed as the integer average
  of alive *human* players). **The asymmetry:** human-facing economic fields read the *owner's own* per-player
  handicap; **every `getAI*` advantage reads the GAME handicap** — so AI research/production/cost advantages scale
  with the *human's* difficulty, never the AI's own (all AIs default `NOBLE`). To make AIs economically stronger,
  raise the human difficulty. Sole exception: `getAIAdvancedStartPercent` reads the AI's own.
- Traps: score multiplies by the raw handicap **enum index** (reordering XML rows silently shifts score); barb spawn
  is **inverted** (a *lower* `getBarbarianCityCreationProb` = a *higher* spawn chance).

## Info loading — `readJson` (the ONE JSON reader) + `CvInfoUtil` (XML residue)

- **The ONE JSON reader ([exactly one JSON reader](../architecture/patterns.md#the-one-reader--the-load-pipeline-law)) is the load pipeline in `Sources/Data/CvReadJson.{h,cpp}`, entry
  point `loadJson()`.** `Assets/Data` is walked, read, and parsed exactly ONCE per process, on first use, into a
  RETAINED in-memory store (~21 MB of JSON text → ~70 MB of picojson structures on the 32-bit heap); every
  downstream step reads the store, never the disk:
  - **Per-category registration** — `CvXMLLoadUtility::LoadGlobalClassInfoJson` is a thin registration against
    the pipeline: at each category's load point in `LoadPreMenuGlobals`/`LoadPostMenuGlobals`,
    `loadJsonCategory` serves the folder's parsed entities in `_order.json` manifest order, and the
    registration assigns ids two-pass (below), dedup-first-wins on colliding types (the trait simple/complex
    share), creates the pocos, and `mapFrom`s each.
  - **The full pass** — `loadJson(JSON_LOAD_PREMENU)` / `loadJson(JSON_LOAD_POSTMENU)` at the END of each XML
    phase: clears the repos, re-registers every store entity (REUSE-ONLY ids — a type is mapped only after its
    registration has landed; pre-registering a postmenu type crashed the load), re-runs the idempotent
    `mapFrom` on EVERY entity against the complete registry, mints + resolves the classification registries,
    runs the FK/reverse passes — whose closing `rp_derive*` sub-passes are the ONE home for a member derived
    from ANOTHER info's edges (`deriveAtRegistryComplete`; the reverse view is final there, so such a member
    materializes once and its getter is a bare read, [materialize at mapFrom](../architecture/patterns.md#materialize-at-mapfrom--no-runtime-string-reads-in-info-getters-the-single-source-laws-load-time-sibling)) — and compiles the
    DepositIndex. The premenu/postmenu PHASING is load-bearing:
    premenu consumers need premenu categories mapped before the menu; the postmenu types
    (processes/votes/espionage-missions/spawns) register late, so the postmenu re-run is what completes every
    cross-category FK edge. The postmenu pass ends by FREEING the store — after load, no JSON-shaped object
    survives.
  - **Fail-loud coverage** — the three failure counts print UNCONDITIONALLY to `Loading.log` on every pass
    (`[READJSON] coverage unresolvedFk=N unconsumedSections=N unknownKeys=N`), plus one
    `[READJSON] ERROR unknown-key` line per non-reserved object key outside the CLOSED family vocabulary
    (`CvJsonParse.cpp` `CJK_FAMILY_KEYS`, mirrored from `Tools/Migration/family_census.py`); the per-item
    detail rides the `SD_READJSON` spine events.
- **`CvInfoUtil`** is the XML loader for the not-yet-replaced info types: one `getDataMembers()` declaration
  derives read/copyNonDefaults/checkSum/init (`CvBuildInfo` is the reference). **The forward direction is
  top-down JSON via `readJson`, which bypasses `CvInfoUtil` entirely** — do NOT chase the old "migrate remaining
  infos to declarative XML" goal.
- The asset **checksum** is serialized and nothing consumes it: it does NOT gate MP OOS, does NOT block loading,
  and no code compares the savegame's value against the current one — so checksum parity is irrelevant when
  restructuring data, at zero cost to an existing save.
  ⚖ **It is WRITE-ONLY state, and it is cut in a FOCUSED PURGE PASS at the end (owner)** — not piecemeal here.
  Removing it is a serialized-member soft-remove ([save.md §3](../specs/save.md): full-delete the read + write,
  name the tag in `Assets/savemigration.txt`), and that discipline is done once, deliberately, across every
  orphaned serialized member together rather than one at a time as each is noticed. ⛔ This is owner-ruled
  SEQUENCING, not a deferral to hide behind: the pass is a named piece of work, and any other write-only or
  consumer-less serialized state found on the way belongs to it — record it here rather than cutting it alone.
  Recorded so far for that pass: `CvPlayer::m_iCompatCheckCount` and `CvPlayer::m_iMotherPlayer` (both
  round-trip the save and are read by nothing).
- **Category id ORDER comes from the `_order.json` manifest** (`Assets/Data/<cat>/_order.json`, curator-derived —
  `Tools/Migration/curate_order.py`): `loadJsonCategory` sorts a category's entities by manifest position before
  the registration assigns ids, so the engine ids reproduce the LEGACY id order (base XML document order, then
  module additions) and every id-ordered UI surface keeps its familiar layout (the level-up promotion popup groups
  each line's tiers adjacently because the XML did). A type absent from the manifest (synthetic `TECH_GAME_START`,
  future additions, a manifest-less category) sorts AFTER every listed one, alphabetically — the legacy
  new-stuff-appends-last behaviour. Manifests are derived artifacts: regenerate + commit freely, never hand-edit.
- **The per-category registration is TWO-PASS by requirement, never one.** Each JSON category loads by (1) registering
  **every** type→id (`setInfoTypeFromString`), then (2) running `mapFrom` on each entity. `mapFrom` resolves its FKs
  at parse time (`jsonResolveId` → `getInfoTypeForString(id, /*bHideAssert*/true)`), so a single register-then-map
  pass silently DROPS any **same-category forward reference** — an id naming a sibling that sorts *after* its owner.
  The miss is invisible (`bHideAssert` writes no `Xml_MissingTypes.log` line);
  it only shows in the FK census (`jsonUnresolvedIds`). This severed ~47% of unit
  `requires.build.dormant`/`replacedBy.units` edges — the entire upgrade/dormancy chain
  (machete→musketman→rifleman→trench_infantry, every trigger sorted after its owner), so no old unit went
  dormant/replaced and the build list showed everything. The two-pass load is the fix; **do NOT collapse it back**.
  Cross-category forward refs (an earlier-loaded category naming a later one — specialist→UNIT, building/unit→
  CIVILIZATION, …) are resolved by the SAME principle one level up: `loadJson`'s full pass re-runs
  the complete `mapFrom` on every entity once ALL categories are registered — `mapFrom` is idempotent by
  contract (`CvInfo.h`), and `/state/info?type=X` is the standing loaded≡authored verification.

## UnitCombat — the fat info class + the cascade-migration note

> Ties directly into the [unit-classification](../specs/skills.md) work — `tags` like `gunpowder`/`mounted` come
> from unitcombats (post-migration).

- **What a UnitCombat IS (owner):** a definition of a unit's **strengths and weaknesses** — the good/bad-against
  column (a shared vs-tag stat bundle), NOT a definition of the unit's TYPE (that is the [tag](../specs/skills.md))
  nor its ABILITIES (those are skills). Three concerns, three homes. This is what it originally was in BTS (a
  vs-based combat grouping); the S2S distillation below restores it.
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
- **The 344 `UNITCOMBAT_CULTURE_*` classes were deleted outright, not distilled to a tag** — each was a redundant
  `{description, culture: BONUS_X}` shell duplicating data already owned by `BONUS_X.enables.units` +
  `identity.bonusClassType: BONUSCLASS_CULTURE`, and `CvUnitCombatInfo::getCulture()` had zero engine consumers
  (unlike `getReligion()`, which is live — hence religion classes stay). This is why `identity.culture` has no
  attach path: there is nothing left to attach.
- **Cascade migration:** a UnitCombat is a modifier SOURCE — its vs-tag stats deposit onto the units that carry it,
  sharing Promotion's modifier-family vocabulary (**do UnitCombat + Promotion together**). Its non-stat content
  distills out: identity → tags, abilities → skills, leaving the pure strengths/weaknesses list. Verify live, then
  purge only vestigial/duplicate classes.
- **⚖ THE LOAD-BEARING DISTINCTION (owner): a TAG is what a unit IS; a UNITCOMBAT is the good/bad-AGAINST column,
  and its "vs" modifiers key on TAGS — never on another unit-combat id.** The canonical pair: **`anti-mounted` is a
  UnitCombat** (the modifier group carrying the bonuses), **`mounted` is a TAG** (the identity of the unit it
  fights). So a vs-modifier authors as `strength.unit.percent {unit: IS_MOUNTED}` **ON** the anti-mounted
  unit-combat — and **the UnitCombat id stops being a modifier TARGET entirely.** Its reason to exist is DRY: author
  "these stats vs these tags" once and attach it to every unit of a kind, instead of duplicating them per unit.
  ⛔ **Promotion prereqs/grants likewise key off the TAG**, not `UNITCOMBAT_*`.
  ⚑ **A unit carries BOTH, permanently — the mapping is ADDITIVE, not a replacement:** a mounted unit keeps its
  unit-combat (the vs-tag stat bundle) *and* has the `mounted` tag (its queryable type), because they answer
  different questions — *how does it fight?* vs *what is it?* A unit's effective tags are its own ∪ its combat
  classes'.
  ⚑ **The payoff is a LARGE purge, and it is GATED, not opportunistic (owner): *"I expect to be able to purge
  literally 100's of unitcombat files eventually, when they stop being used as identifiers, but we are not there
  yet."*** That names the dependency exactly — the proliferation exists because the combat-class enum doubles as an
  IDENTIFIER (the size/species/weapon/motility taxonomy). Once TAGS carry identity, every class that existed only to
  identify becomes dead weight and goes. ⛔ So the purge follows the tag re-expression; purging ahead of it removes
  classes still doing identifier duty (the blunt purge that over-reached and was reverted).
  ⚖ **This is the GOAL, not the now (owner) — and TAGS AND UNITCOMBATS LIVING SIDE BY SIDE IS SANCTIONED, not a
  half-state to fix.** The shipped data still keys vs-entries by `UNITCOMBAT_*` ([skills.md](../specs/skills.md) §1
  documents that current shape) and that is FINE: *"there is nothing stopping us from letting tags and unitcombats
  live side by side."* Actually solving the re-expression needs its own **post-rework dedicated pass**, so the two
  shapes coexist until then. ⛔ Do not read the coexistence as drift, and do not "converge" them opportunistically
  mid-rework — the additive model above is exactly why coexistence costs nothing.
  ⚠ **Meeting the gate is NOT a green light, and this is the trap to name explicitly.** Tags taking over the
  identifier role was the purge's stated precondition, so as identity tags land it starts to *read* as permission
  to begin purging — it is not. The purge is a separate, owner-scheduled piece of work, never a live worklist to
  act on now.

## See also

- [../specs/](../specs/) — the cascade model the engine feeds. [../spine.md](../spine.md) — the
  map-before-delete + observability bar that gates cutting any legacy maintainer above.
