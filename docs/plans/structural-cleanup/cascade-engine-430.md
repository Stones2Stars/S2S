# Cascade engine (#430) — implementation plan

> ## ⛔ RESUMING AFTER A CONTEXT COMPACTION? RE-READ EVERYTHING FIRST.
>
> If your context was just compacted mid-session, do NOT act from the summary. Re-read the full spec set (the machine
> specs `enabler.md` / `modifier.md` / `tally.md` + `event-spine.md` + this doc + `json.md`) AND the live code in
> `Sources/Cascade/` before touching anything — compaction has poisoned context before; a stale line loses to a later
> owner ruling AND to the live code. (Owner 2026-06-17; this gate is deliberately tested.)

**Status: PROTOTYPE PURGED — restarting on a proper footing.** The first cascade + tally + modifier prototype was
written, shadowed, and then **removed** (it predated a proper BoolExpr-routed `readJson` and the decision to validate
via the external StoneBase dry-calc — removal tombstones at `CvGame.cpp:5822`, `CvEventSpine.cpp:301-302`). Rebuilt
fresh since (this branch): the **event spine** + the **tally** (`CvCascadeTally`, shadow-green) + **`readJson`**
(`CvCascadeReadJson`, full-coverage parse/survey — but NOT yet mapping JSON→runtime data) + the building/unit count
`DOMAIN` emits + the legacy `#195` reverse-index overlay. The modifier/enabler/grants machines are NOT built. The DESIGN is
authoritative in the machine specs — `enabler.md` + `modifier.md` + `tally.md` + `event-spine.md` — and the reference
IMPLEMENTATION is now **StoneBase** (the .NET dry-calc, parity-validated for city yields + commerce). This doc is the
engine BUILD plan + the validated demolition map of the **legacy** machinery the cascade replaces; it is **not** a
status record of built cascade code (there is almost none — see the table).

---

## ⓘ IMPLEMENTATION STATUS — re-verified against `Sources/` on 2026-06-29 (post-purge)

> The HONEST ground truth. The previous status table self-certified a 2026-06-19 sweep and described a prototype that
> has since been PURGED; it paraded retired/banned machinery as built. This table is the post-purge reality from an
> adversarial docs-vs-code sweep (2026-06-29, fanned out, every claim ground-tested). The machine specs describe the
> TARGET shape; almost none of it is built in the engine yet.

| Component | Status | Reality (current `file:line`) |
|---|---|---|
| **Event spine** | ✅ **BUILT (spine + logging only)** | `Sources/Cascade/CvEventSpine.{h,cpp}`: `IEventConsumer` (`CvEventSpine.h:197`), `CvEventSpine` (`:208`), `eventSpine()` (`:224`), `cascadeRegisterConsumers()` (`:229`) registers ONLY the log consumer (`CvEventSpine.cpp:300`). KINDs `DOMAIN`/`DIAGNOSTIC`/`TRACE` (`:28`). **The only DOMAIN (counted) events** = `CASCADE_EVT_BUILDING_COUNT`/`UNIT_COUNT`/`NAME_CHANGE` (`:171`), emitted at `Engine/CvPlayer.cpp:13737` / `:13642` and `Engine/CvCity.cpp:13391`. The `[TAG]` logging domains (`SpineDomainTag`, `:73`) are mostly pre-allocated; only AI-trace domains (hunter/worker/war/…) self-register today. |
| **Scoped accumulator** | ❌ **NOT BUILT (purged)** | The prototype's `CvScopedAccumulator` was removed; no such file exists. Design = the additive scope-accumulator substrate (§1.0). |
| **Tally** | ✅ **BUILT (buildings + units, shadow-green)** | `Sources/Cascade/CvCascadeTally.{h,cpp}`: `IEventConsumer` with `wantedKinds()=1<<EVENTKIND_DOMAIN`; `rebuild()` seeds from the live objects, `onEvent` maintains it, `shadowDiff()` emits per-turn `[TALLY/shadow]` (verified `diverging=0`). Registered at `cascadeRegisterConsumers()`. Player-leaf model (`tally.md` §2). Building/unit domains only; other domains pending (`tally.md` §5). |
| **Modifier** | ❌ **NOT BUILT (purged)** | Design = `modifier.md`; reference impl = StoneBase (`src/Application/Features/Calc/*` over `ModifierMath`, parity-proven). Legacy accumulators fully present + untouched (§4). **NEXT machine** — consumes readJson's modifier-family deposits; needs the scope-accumulator substrate (§1.0). |
| **Enabler** | ❌ **NOT BUILT (purged)** | Design = `enabler.md`. Legacy `can*` gates fully present + untouched (§4). The `#195` `ConstructRequirement` reverse-index overlay survives (`Engine/ConstructRequirement.h`; built in `Defines/CvGlobals.cpp`, consumed `AI/CvCityAI.cpp:12965`) and is the seed of the `enables` generation index. |
| **Grants** | ❌ **ABSENT** | No grants consumer (never built). |
| **readJson** | ✅ **BUILT — PARSE/SURVEY (full json.md coverage); ⛔ does NOT yet MAP to runtime data** | `Sources/Cascade/CvCascadeReadJson.{h,cpp}`: a one-shot gated probe (`cascadeReadJsonProbe`, at `doTurn`) that parses EVERY `Assets/Data` entity through BoolExpr — `rj_translate` (conditions, incl. `IntExpr` value/tally-count bands + the `dormant` clause); `rj_walkModNode` (×100 modifier-family deposits); `rj_walkEnableEdge` (enables/obsoletes/replaces/disables/obsoletedBy/provides); `rj_walkAllowed`; `rj_walkGrants` (generic by shape) — FK-resolving all ids. **Verified live: 0 `UNCLASSIFIED` top-level keys (complete coverage), all resolved bar 3 by-design.** ✅ It now **MAPS** the parse to a `CvCascadeData` per entity (deposits/requires/edges/allowed/grants) and attaches it via the ABI-safe **side-table** (`cascadeForInfo`/`cascadeAttach`, keyed by `CvInfoBase*` — NOT a `CvInfo` member; CvInfoBase is EXE-layout-bound, §3). Live read-back round-trips it (`[READJSON/map-summary] entitiesWithCascadeData≈13k`). The modifier/enabler calc reads this mapped data. Standalone `Tools/ReadJson/readjson.cpp` harness is FROZEN. |

**The honest #430 roadmap.** Build the cascade FRESH per the specs, porting from the parity-proven StoneBase reference,
in spec build order: **`readJson` (BoolExpr-routed) + the scope-accumulator substrate → tally → modifier → enabler →
grants** (§1). Each is interface-bounded and wired at the composition root (§2b). Validation has **two distinct legs,
never mixed** (§2): the **external StoneBase dry-calc** (offline PARITY oracle — done for yields/commerce) and, per
machine, an **in-engine SHADOW** (the cascade computed alongside legacy, diffed per-turn via spine `[TAG]` lines — NOT
a `/shadow/*` endpoint; those were a rollerskating conflation of the two legs and are retired). Nothing below "event
spine" exists yet.

---

## 1. THE ROOT SYSTEM — one substrate, three machines (owner 2026-06-16)

The new system roots in a shared **scope-accumulator substrate** with **three machines** on it. They are not three
tangles — they are three instantiations of one primitive, which is what makes the engine coherent. (DESIGN; only the
event spine of §1.0 is built.)

### 0. Substrate — the scope spine + an additive accumulator + the EVENT SPINE (interface-bounded)

- **Scope spine:** `world → team → empire → area → city → plot{improvement|feature|terrain|route} → building | specialist | unit`.
- **Additive accumulator:** deposit → O(1) summed read, parameterized by what it sums. One primitive, three instances.
  The enabler additionally walks the SIDEWAYS/progression axis (tech tree, build chain — `enabler.md` §2); modifiers
  are containment-only. **(NOT BUILT — the prototype's accumulator was purged; rebuild fresh.)**
- **Event spine — the dispatch FRONT DOOR (`event-spine.md`).** `emit(KIND, …)` → consumers read the kinds they care
  about. The tally, `grants`, and logging are all CONSUMERS; the spine sits IN FRONT OF the tally (the tally never
  reaches into game state — domain events come to it). **(BUILT: `Sources/Cascade/CvEventSpine.{h,cpp}`; only the
  logging consumer is registered. The tally/grants consumers are unbuilt.)**

### 1. Tally — counts, roll UP  *(build FIRST after substrate; spec: `tally.md`)*

Per-type had-counts, additive roll-up the spine. Serves: `requires` count-thresholds (`min(BUILDING_X,12)`,
empire/team) + the higher-scope HAS sets, the modifier's cross-city `per` count-scaler, and demographics/AI/score.
First because the enabler depends on it (`tally.md` §2/§3). **Player-leaf** stored model (`tally.md` §2). Serializes
nothing — rebuilt on load (`tally.md` §4).

### 2. Modifier — magnitudes, deposit DOWN

Per `(family, member, unit, scope)` summed deposits; targets read O(1).
`effective = (base + Σflat) × (100 + Σpercent)/100 × Π(multiplier/100)` (`modifier.md` §2). The realized per-channel
yield/commerce RATE has the two-tier BASE/AFTER shape of `modifier.md` §2a. Replaces the CvCity yield/commerce/health/
happiness/defense/maintenance accumulators + the `process*` apply-loops + the unit extra-stat stack (§4). The StoneBase
`Calc` packages + `ModifierMath` are the parity-proven blueprint for the C++ port.

### 3. Enabler — availability, 2-pass

gather **HAS** (reads the tally for higher scopes) → generate **CAN GET** (the `enables`-family forward index) → gate
**`requires`** (`enabler.md` §1-3). One shared frontier, read by UI greying + AI `doProduction`. Replaces the scattered
`can*` gates + the PreLoop + the caches (§4). NB the `replaces` edge is now **LIVE** (unit succession — `enabler.md`
§2/§ units), no longer a reserved-but-unused family.

### 4. readJson — the DATA INPUT *(owner 2026-06-16: "we HAVE to do this before we go anywhere")*

A FRESH picojson reader that parses the full new vocabulary (`enables`/`obsoletes`/`requires` trees — plus `replaces`;
the modifier families `<family>.<scope>[.<member>].<unit>`; `grants`; the predicate tokens; count atoms; scopes) into
the runtime structures the three machines consume. The JSON conditionals are **routed through `BoolExpr`** (§2b) — the
prior prototype's hand-rolled `vector<vector<leaf>>` was exactly the AND-of-ORs mistake `enabler.md` §3.1 warns against.
`readJson` is a pure CONSUMER of the modder-authored shape (defined by `json.md`, not by `readJson`). **(NOT BUILT —
prototype purged; this is the first build item — detailed build plan: [`readjson.md`](readjson.md).)**

**Build order: `readJson` + substrate → tally → modifier → enabler → grants.** Each interface-bounded, each deleting
its slice of the demolition map (§4) as it lands and passes its shadow.

---

## 2. DEVELOPMENT STRATEGY — two validation legs, never mixed; the hard JSON switch is LAST

The key to building a large atomic replacement safely: **don't build it blind in one shot.** There are **two distinct
validation legs** (the previous agent collapsed them into one `/shadow/*` endpoint surface — wrong on both axes; that
surface is retired, `CvHttpServer.cpp:26`):

- **PARITY — external, offline (the StoneBase dry-calc).** A separate process/codebase (`StoneBase`, .NET) rebuilds
  the cascade from the curated `Assets/Data` JSON + raw `/state` and diffs against the engine's `/computed` oracle. It
  is the spec's reference implementation and the C++ port's blueprint; **done for city yields + commerce** (the
  packages + `ModifierMath`). See `validation.md`.
- **SHADOW — in-engine, live (per machine, as it is ported).** Each rebuilt machine runs **alongside** the legacy
  machinery behind gated logging (off in normal play), emitting a per-turn cascade-vs-legacy diff as spine `[TAG]`
  lines (the `event-spine.md`/`logging.md` surface — **not** a `/shadow/*` endpoint, **not** the banned care-scale
  grading). The legacy stays authoritative until its shadow is **clean (parity)**, then it is cut.

- **Atomic-deliverable rule:** the DATA cutover stays atomic (one flip at the very end); the ENGINE is developed
  incrementally and shadow-validated first. The new paths are gated instrumentation until the flip.
- **Shadow data source = PER MACHINE (corrected 2026-06-29).** The **tally** reads raw object counts
  (`m_paiBuildingCount`, …) + the spine's `DOMAIN` events — **no JSON** — so it shadows directly against the live
  objects (apples-to-apples, isolating the count-machine logic; it needs no `readJson`). The **modifier + enabler**
  read `readJson`'s JSON-parsed structures (they consume the family/tree vocabulary the XML objects don't carry) — and
  the JSON-vs-XML data equivalence is exactly what the StoneBase dry-calc already proved offline, so this is sound. The
  XML `read()` path stays authoritative for the EXE-bound accessor surface until the atomic cutover (§3). Build FRESH;
  actively AVOID `CvInfoUtil`/the old `read()` path (§2b).
- **Exact parity IS the success metric** ([DEC-parity](../../architecture/decisions.md#dec-parity) /
  [DEC-mirror-then-redesign](../../architecture/decisions.md#dec-mirror-then-redesign)): the migration MIRRORS the
  engine exactly. A surviving divergence is a data-collection gap (or a new-side bug), mapped to its named source and
  closed — never tolerated as a formula difference, never graded on a "care scale" (banned). Per
  [DEC-no-parity-results-in-docs](../../architecture/decisions.md#dec-no-parity-results-in-docs), divergence NUMBERS
  live in the run, never in this doc.
- **The `readjson.exe` cleartext RENDER is the JSON-INTENT surface** (`Tools/ReadJson/readjson.cpp`): `--render TYPE`
  states in plain English what an entity's JSON *says* it does. That is the intent; the StoneBase parity + the in-engine
  shadow are what the engine *actually does*. A render/behaviour divergence is a triage item.
- **The hard switch (last step):** `readJson` replaces `readXml` at the load seam (§3) **and** the rebuilt machines
  become the sole source as the demolished machinery (§4) is deleted. One atomic landing.

---

## 2b. BUILD PRINCIPLES — from scratch, interface-bounded, poor-man's DI (owner 2026-06-16, 2026-06-29)

- **★ THE MOD FITS THE NEW STRUCTURE — NOT THE REVERSE.** The cascade data model + engine are authoritative; the mod's
  data/content is RESHAPED to fit them. Two guardrails: (a) **PRESERVE SAVES** where possible (name-tagged soft
  format; `@SAVEBREAK` only when unavoidable); (b) **PRESERVE HOW THE GAME WORKS** (the intended player experience
  stays recognizable). Structure & internals are free; the played game is not.
- **Build the components FROM SCRATCH, interface-bounded.** `readJson` + tally + modifier + enabler + grants are
  written fresh as `IEventConsumer`/contract implementations — never threaded through `CvInfoUtil` / the old `read()` /
  `SetGlobalClassInfo` (demolition fodder, §4, both still present: `CvInfoUtil.h`, `Infrastructure/CvXMLLoadUtilitySet.cpp:1516`).
- **Poor-man's DI — the composition root (owner 2026-06-29; the entire DI/composition setup is in scope).** No DI
  container exists (C++03/VC7.1; the EXE binds concretes), so depend on **interfaces** (pure-virtual base, no data —
  `IEventConsumer` is the realized exemplar at `CvEventSpine.h:197`), hold a **pointer to the interface**, and pick the
  concrete with a literal `if`/`switch` at the **composition root**. `cascadeRegisterConsumers()` (`CvEventSpine.cpp:292`)
  IS that root for the spine's consumers — extend it as the tally/grants are built. The full shape + guardrails:
  [`architecture/patterns.md`](../../architecture/patterns.md). The canonical option-gated swap is the trait
  simple/complex split (§7).
- **`picojson` for JSON** (header-only) — `Sources/include/picojson.h`, included via the PCH umbrella
  (`CvGameCoreDLL.h:310`), already proven (it backs `CvHttpServer`). Reuse it; don't bend the XML machinery to JSON.
- **`BoolExpr` for the conditionals (DELIBERATE REUSE).** The JSON conditionals (`all`/`any`/`noneOf` over atoms +
  predicates) are isomorphic to the engine's `BoolExpr` (And/Or/Not over Has(GOM)/Is(tag)) — `Sources/Infrastructure/BoolExpr.{h,cpp}`
  (classes at `BoolExpr.h:91/113/143/164`; parsed at `CvXMLLoadUtilitySet.cpp:1579`; widely used, ~44 files). `readJson`
  translates a JSON conditional directly into a `BoolExpr` tree. The isolated `Tools/ReadJson/` harness can't link
  `BoolExpr`, so it proves the same parse by rendering to clear text. One conditional shape, two back-ends.
- **The derived-data REPOSITORY (`Engine/CvDerivedData.h` — classes `CvDataRepository<T>` / `CvGameDataRepository` /
  `TLazy`; the *file* is `CvDerivedData.h`, there is no class of that name) is NOT built upon — and NOT removed yet.**
  Its `init()`/`reset()` are still wired in the lifecycle (`CvGame.cpp:62`/`:898`, `CvPlayer.cpp:160`/`:763`,
  `CvCity.cpp:55`/`:475`, `CvTeam.cpp:36`/`:190`). The cascade's substrate accumulator is authoritative additive
  aggregation and borrows none of the repository's lazy/dirty machinery; removal of the empty skeleton is a §4
  demolition item, deferred to cutover.
- **The ONLY shared/kept pieces are the hard EXE boundary (§3)** — the type registry + the EXE-bound accessor surface.
  Everything else is fresh.

---

## 3. EXE BOUNDARY — the only fixed constraint (re-verified 2026-06-29)

- **`readJson` will be a FRESH reader** (picojson → fresh runtime structures), NOT a reuse of `CvInfoUtil`/`CvXMLLoadUtility`/
  the old `read()` path. It runs IN ADDITION to the XML load during shadow; at cutover the XML path (`SetGlobalClassInfo`
  → `read()`, `Infrastructure/CvXMLLoadUtilitySet.cpp:1588`) is deleted. (`read()` is NOT DllExport — `Infos/CvInfoBase.h:78/85/133/154`.)
- **★ THE MAPPING/CUTOVER MODEL (owner ruling 2026-06-29; ABI-corrected after a load-crash).** `readJson` maps each
  entity's JSON to a fresh **`CvCascadeData`** (deposit lists, the `BoolExpr` requires/enables structures, the grant
  provisions) — the **new calc implementations** (modifier/enabler/grants) read it; the **OLD Info variables**
  (XML-populated `m_ai*`/`m_ab*`) are **cut only AFTER** (a) the data is mapped, (b) the new calc is built, and (c) the
  per-machine SHADOW reaches parity. Until all three hold the XML load stays authoritative (cutting now breaks the game
  — nothing maps JSON→engine data yet); the cut is the atomic last step.
  - **⛔ HOME = real indexed structs on the INFO objects (owner ruling 2026-06-30).** Terminology (the owner pinned it):
    **`CvInfoBase`** is the base of the **JSON-data-derived objects** — the infos (`CvBuildingInfo`/…), where the
    type-data maps; **`GameObject`/`CvGameObjectCity`** is the base of the runtime **INSTANCES** (city/unit), which
    conditions EVALUATE against (the active-state side). The cascade **type-data lives on the infos**:
    - **Universal structures every info needs — `modifiers` + `enables` — are BASE members on `CvInfoBase`** (appended);
      type-specific data is **per-derived** (`CvBuildingInfo`/…). The standard EXE-required info fields (`type`/
      description/the DllExport getters) are **REUSED** from the info, never duplicated — this carries only new data.
    - **They are REAL indexed structs/classes that GIVE THE ANSWER directly** — `modifiers` keyed by family/scope/unit,
      `enables` by bucket — NOT a flat list linear-scanned everywhere (that is StoneBase's `ModifierFamily` *tree* shape,
      which the interim flattened). The machine asks the struct and gets the value; no full-list walk per access.
  - **ABI — the load crash was a MID-CLASS insertion, NOT appending.** Adding `m_pCascade` *before* the existing
    `CvInfoBase` members shifted `m_szType` (which the EXE reads by a hardcoded offset) → the `memcpy` AV in
    `std::string::assign`. **Appending to the END preserves every existing offset** (the standard C2C way of extending an
    info), so base members on `CvInfoBase` are viable — **confirm with an append-test** (a base append also shifts the
    *derived* members; safe iff the EXE reads those via getters, not by offset — verify). Access is **typed** (readJson +
    the machines dispatch by `GC.get*Info(id)`), so no base virtual getter is needed.
  - **Why NOT the side-table/dictionaries:** StoneBase used dictionaries only because it is **offline** (perf secondary)
    with **no engine base objects** — it builds its own typed model. **The DLL HAS the real info objects**, so map ONTO
    them: a direct member is **faster** (no map lookup in the modifier's hot loops) and makes **provenance obvious**.
  - **⏳ CURRENT (interim, to replace):** `CvCascadeData` is a **flat deposit vector** in a `CvInfoBase*`-keyed
    **side-table** (`cascadeForInfo`/`cascadeAttach`) — the over-correction after the mid-class crash (+ echoing the
    StoneBase dicts). It WORKS + is verified (`[READJSON/map-summary]` round-trips, game loads clean,
    `[TALLY/shadow] diverging=0`), but it is the slow/opaque shim. Redesign → real `modifiers`/`enables` structs as
    appended base members on `CvInfoBase` (+ per-derived for type-specific), read directly.
  - **★ TWO HOMES — definitions on infos, ACCUMULATED STATE on instances (owner ruling 2026-06-30).** The static
    **definitions** (the `modifiers`/`enables` structs from JSON) live on the **infos** (above). The runtime
    **accumulated state** — the cascade accumulators (a city's summed per-`(family,scope)` totals) + the **tally**
    counts — naturally lives on the **game INSTANCE objects** (city / player / unit). Per §2b those go on the
    **DLL-internal derived** instance classes (`CvCityAI`/`CvPlayerAI`/`CvUnitAI`), **never** the EXE-bound bases
    (`CvCity`/`CvPlayer`/`CvUnit`). **NO dictionaries/side-tables** (a StoneBase offline-ism — it had no engine
    objects; the DLL does, so state lives ON them). ⏳ This retires BOTH current dict-interims: the modifier
    **side-table** (→ onto the infos) AND the **tally** singleton-maps (→ onto the player instances).
- **The shared/kept pieces (EXE-bound):** the **type registry** `GC.getInfoTypeForString` (`Defines/CvGlobals.cpp:2682`,
  decl `CvGlobals.h:1418`) / `setInfoTypeFromString` (`CvGlobals.cpp:2708`, decl `:252`) over `m_infosMap`
  (`CvGlobals.h:929`) — `readJson` uses it for FK resolution because the EXE binds the same indices; and the **EXE-bound
  accessor surface**: `CvInfoBase` DllExport getters `getType`/`getTextKeyWide`/`getDescription`/`getText`/`getHelp`
  (`Infos/CvInfoBase.h:55/71/72/73/75`), the `getNum*Infos()`/`get*Info()` pairs, a few art getters. During shadow the
  OLD objects serve the EXE; at cutover the fresh structures must serve that surface (or it is reworked) — a cutover
  detail. Everything outside this boundary is freely built fresh.

---

## 4. DEMOLITION MAP — what the engine deletes/rewires (re-verified 2026-06-29)

> All legacy paths below are **present and untouched** — the cascade replacement has not happened. Paths are now
> subdirectory-qualified (the tree was reorganized into `Sources/{Engine,AI,Tools,Infos,Defines,Infrastructure,UI,Python}/`);
> line numbers re-verified 2026-06-29 (±, they drift as the files change).

**Enabler (→ §1.3 machine):** `CvCity::canConstruct`/`canConstructInternal` (`Engine/CvCity.cpp:2496`/`:2557`) +
`CvPlayer` (`Engine/CvPlayer.cpp:6513`/`:6572`); `canTrain` (`Engine/CvCity.cpp:2357` + the UnitCombat overload `:2453`;
`Engine/CvPlayer.cpp:6374`); `canEverResearch` (`Engine/CvPlayer.cpp:8260`, `Engine/CvGame.cpp:11276`); `canDoCivics`
(`Engine/CvPlayer.cpp:8449`); `canFoundReligion` (`Engine/CvPlayer.cpp:10105`); `canCreate` (`Engine/CvCity.cpp:3034`,
`Engine/CvPlayer.cpp:6804`); `canFound` (`Engine/CvPlayer.cpp:6199`). Caches: `CvPlayer` `m_bCanConstruct*` (4 arrays,
`CvPlayer.h:2404-2407`, cleared `:28056`/`:28076`); `CvCity` canTrain cache (`m_canTrainCacheUnits`/`…Populated`,
`CvCity.h:2014`/`:2017`, gated `#ifdef CAN_TRAIN_CACHING`) + the `m_bCanConstruct` map (`CvCity.h:2029`, flush
`CvCity.cpp:2474`); the `VALIDATE_BUILDING_CACHE_CONSISTENCY` shadow check (`AI/CvCityAI.cpp:4829`/`:5158`).
`CvCityAI::CalculateAllBuildingValues` PreLoop (`AI/CvCityAI.cpp:12839`, PreLoop profile `:12962`, body runs past
`:14278`). `ConstructRequirement` + the **#195 enabler index** (**partly KEPT** — it *is* the `enables` generation
index seed: `Engine/ConstructRequirement.h`, built in `Defines/CvGlobals.cpp`). `setHasBuilding` extension/replace
chain-walk (`Engine/CvCity.cpp:14563`; extensions `:14609-14622`, replace `:14625-14654`).

**Modifier (→ §1.2 machine):** the CvCity yield/commerce/health/happiness/defense/maintenance accumulator arrays
(`Engine/CvCity.h`: `m_aiBaseYieldRate:1747`, `m_aiExtraYield:1759`, `m_aiYieldRateModifier:1762`,
`m_aiCommerceRate:1769`, `m_aiCommerceRateModifier:1782`) + `getBaseCommerceRateFromBuilding100` (`Engine/CvCity.cpp:12357`);
`processBuilding` (`Engine/CvCity.cpp:4525`, body ends ~`:5145`) / `processSpecialist` (`:5157`) / `processBonus`
(`:4421`); `CvPlayer::setCivics` (`Engine/CvPlayer.cpp:14289`) / `processTrait` (`:28449`); `CvTeam::processTech`
(`Engine/CvTeam.cpp:5902`) / `CvPlayer::processTech` (`Engine/CvPlayer.cpp:30909`); the `CvUnit` `changeExtra*` stack
(**exactly 91 setters**, `Engine/CvUnit.cpp:11386…30937`). *(The old map's `processCorporation` and `getBuildingYield`
do not exist — removed/never-present; the real building-yield getters are `getBuildingYieldModifier`/`…Change`.)*
*(Active-rework signal: `m_aiBuildingBonusVicinityYield100` (`CvCity.h:1749`) is a newly-added recompute-only,
non-serialized field — the vicinity-build-order fix; this accumulator block is being reworked, not frozen.)*

**Tally (→ §1.1 machine):** the cross-city `getNum*` prereq count loops inside the gate functions
(`CvPlayer::getBuildingPrereqBuilding` `:7306` using `getBuildingCount` `:7329`; call sites `CvPlayer.cpp:6718`/`:6783`,
`CvCity.cpp:1504`) + the demographics/score scans (`CvPlayer::calculateScore` `:4417`, `getPopScore`/`getLandScore`/
`getWondersScore`/`getTechScore` `:11506`/`:11546`/`:11587`/`:11610`) become reads of the one tally **(target machine
not yet built — this clause names the legacy code the tally will subsume, not existing tally consumers).**

---

## 5. HARD BOUNDARIES (cannot rewire)

- **EXE ABI** (§3) — the closed Firaxis `.exe` binds the DllExport surface + base classes.
- **Save format** — name-tagged (`CvTaggedSaveFormatWrapper.h:14-17`); removing a serialized member is soft; intentional
  breaks → `@SAVEBREAK`. Derived/accumulator state serializes nothing (recomputed on load).
- **OOS / lockstep determinism** — integer math only; synced Soren RNG. `readJson` **will convert** readable→`int×100`
  ONCE at load (deterministic). No runtime float. Canonical: [DEC-fixedpoint-x100](../../architecture/decisions.md#dec-fixedpoint-x100).
- **Toolchain** — C++03, 32-bit/x86, vendored VC7.1, Python 2.4, Boost 1.32/1.55, raw Win32 (no `std::thread`/C++11+).
  (`Sources/fbuild.bff:7/47/48/325`.)

---

## 6. THE TRAIT SIMPLE/COMPLEX SPLIT — engine fix (owner ruling 2026-06-29)

Legacy still resolves traits via the `CvInfoReplacements` runtime hack and **must** be moved to the option-selected
curated-set model as part of this migration. The current mechanism (mapped 2026-06-29):

- **The swap:** `CvInfoReplacements<T>::updateReplacements` overwrites `aInfos[id]` with the first replacement whose
  `BoolExpr` condition holds (`Infos/CvInfoReplacements.h:191` — backup `:190`, restore `:173`; condition `m_pCondition`
  `:27`, evaluated `:63`). The driver `cvInternalGlobals::updateReplacements()` (`Defines/CvGlobals.cpp:753`) fans out
  to 36 per-type tables incl. **`m_TraitInfoReplacements.updateReplacements(m_paTraitInfo)`** (`:759`). Re-run on state
  changes at `CvGame.cpp:238`/`:1124`/`:11572`. XML parse of `ReplacementID`/`ReplacementCondition` at
  `Infrastructure/CvXMLLoadUtilitySet.cpp:1568-1584` (`addReplacement` `:1604`).
- **The target.** The curator already emits two self-complete folders (`traits/simple/` + `traits/complex/`); the
  engine should **load the one active set** chosen by `GAMEOPTION_LEADER_COMPLEX_TRAITS` and inject it behind a trait
  query-surface at the composition root (poor-man's DI, §2b) — the exact "isolate systems → both implement one contract
  → switch at the root" worked example in `patterns.md`. This is the StoneBase `ModifierMath.ActiveTraitSet` model
  ported into the engine. The `CvInfoReplacements` trait swap (and its per-turn re-run for traits) is then demolition
  fodder. (`modifier.md` §4 owns the curator-side CRAZY→sensible translation + the CLEAN gates the cascade applies;
  this §6 owns the engine-side replacement-hack removal.)

---

## 7. NEXT

1. **`readJson` (BoolExpr-routed)** ✅ **PARSE + MAP DONE** — full json.md coverage; maps each entity's JSON to a
   `CvCascadeData` attached by game object via the ABI-safe side-table (`cascadeForInfo`). ⛔ **NOT yet mapped: the
   classification blocks** (`skills`/`tags`/`capabilities`/`state`, json.md §8) — currently recognized + skipped. See the
   classification-wiring item below.
2. **Tally** ✅ **DONE** (buildings + units) — player-leaf, rebuild-on-load, first `DOMAIN` consumer; live `[TAG]`
   shadow `diverging=0` against the legacy count scans. Other count domains pending (`tally.md` §5).
3. **Modifier** — build plan [`modifier-machine.md`](modifier-machine.md). The readJson→`CvCascadeData` mapping is DONE;
   the **percent stack** (increment 1) is in + attributed (building tier bit-exact). **Strategy (owner ruling 2026-06-30):
   port the WHOLE StoneBase `Calc` in, THEN compare the in-DLL shadow vs StoneBase's parity-proven results** (port
   fidelity) — do NOT chase per-increment parity. **Parity = full ATTRIBUTION + a showable diff, NOT bit-exact**
   (validation.md; the specialist-bucket move makes bit-exact impossible; StoneBase proved attribution-parity is reachable).
3a. **★ WIRE THE NEW CLASSIFICATION + POLICY BLOCKS so the engine consumes them exactly as it uses the legacy flags
   today (owner ruling 2026-06-30 — SUPER IMPORTANT: these easily get LOST, and a wrong/missing one DETERIORATES the
   game experience).** Map `skills` / `tags` / `capabilities` (json.md §8) + `policies` (§9, civic law toggles) onto the
   game object as the new arrays, and make each engine SYSTEM read the new array the same way it reads the legacy
   per-flag: a unit's `skills.blitz` → multiple-attacks (as the legacy blitz); empire `capabilities` → the team ability;
   `policies.noForeignTrade` → the trade-route engine; unit `tags` → the `IS_<TAG>` accounting. **empire capabilities +
   unit skills especially.** The classification blocks are currently parsed-but-skipped (not mapped) — this is the wiring.
4. **Enabler** (generate-then-gate, on the validated tally) + **grants** — **build EARLY, a CO-REQUISITE with the
   modifier (owner ruling 2026-06-30), not a later step.** Without the enabler the cascade does not know **what is
   ACTIVE** — which bonuses are connected/available, which buildings are non-dormant — and the modifier's conditions
   (`enabled:{HAS_BONUS}`, `connection:vicinity`, dormancy) depend on exactly that. **The modifier shadow must read the
   ENABLER's active state, NOT the live engine (owner ruling 2026-06-30)** — so the cascade is self-contained (it must
   keep working after the legacy state is cut); the enabler's active state is **independently shadowed vs the engine to
   prove they are EQUAL** (StoneBase proved this is achievable). (The current percent-stack reads the live engine as an
   INTERIM shim until the enabler exists — modifier-machine §2.) So sequence the enabler alongside the modifier.
5. **The trait simple/complex engine fix (§6)** — retire the `CvInfoReplacements` trait swap for option-selected
   injection — sequenced with the modifier/enabler work (it changes which trait values both read).
6. **The atomic cutover** — `readJson` replaces `readXml`; the demolished machinery (§4) is deleted in one landing.

Each machine carries the alignment fixes it touches as it is wired. Parity is proven offline by StoneBase and live by
the per-machine shadow before anything legacy is cut.
