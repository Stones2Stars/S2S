# Unified Prerequisites & Constructibility (#195)

> This is the **plan / rollout / measurement history**. For how the shipped system works
> today (the model, the enabler index, help-text rendering), see the reference note
> [`../reference/constructibility-and-prerequisites.md`](../reference/constructibility-and-prerequisites.md).

**Status:** Phase 1 COMPLETE & merged (PR #314). Phase 2 (unified model + index + help-text)
on PR #315, user-verified. Index is authoritative; legacy O(buildings²) scan removed.

## Current state (what's built)
- `BoolExpr::getInvolvedGOMs(vector<GOMQuery>&)` — one-pass gather visitor that
  appends every (GOM,id) leaf a condition references (`Sources/BoolExpr.{h,cpp}`).
  Mirrors `getInvolvesGOM`; only `BoolExprHas` contributes, composites forward.
- `cvInternalGlobals::buildConstructibilityEnablerIndex()` +
  `getBuildingsEnabledBy(BuildingTypes)` / `getUnitsEnabledBy(BuildingTypes)`
  (`Sources/CvGlobals.{h,cpp}`) — two static reverse-indices, built once at the end of
  `doPostLoadCaching` from typed prereqs + free-bonus givers + each condition's involved
  GOMs. Building index: enabler B → buildings it can make constructible. Unit index:
  enabler B → units it can help train (mirrors the CABV unit-enabler value loop's static
  triggers — PrereqAndBuildings, train-condition GOMs, free-bonus bonus needs).
- **Building set** (`CvCityAI.cpp` PreLoop): builds the constructible set directly from
  `getBuildingsEnabledBy` (O(dependents) per enabler), same `canConstructInternal`
  confirm. The old O(buildings²) inner re-scan + `EnablesOtherBuildings()` gate are gone.
- **Unit enablers** (`CvCityAI.cpp`, both the cached and AI_buildingValue loops): iterate
  `getUnitsEnabledBy(eBuilding)` instead of all units; the runtime gates (tech, hasBonus,
  isActiveBuilding) are unchanged, so the value result is identical.

**Verification (done):** shadow/superset checks under `gPerfLogLevel >= 1` over a real
game (turns 1336–1337, multiple cities, both AI, 5202 buildings, constructible sets
55–440, enabler-added up to ~60/set) logged **0 `MISMATCH` and 0 `UNIT-MISMATCH` across
145 cache rebuilds** → index sets byte-identical to the legacy scan; unit index a
confirmed superset. The shadow/verify code was then removed and the index made
authoritative. The `[PERF/cabvset]` summary line is kept (gated) for turn-time tracking.

**Measured turn-time win (same save, FinalRelease, per-rebuild mean / max):**
| | BEFORE (O(buildings²)) | AFTER (index) | speedup |
|---|---|---|---|
| Building PreLoop | 123.82 ms / 1042 ms | 0.32 ms / 0.99 ms | ~390× / ~1050× |
| Unit-enabler loop (`notdev`) | 0.53 ms / 2.70 ms | 0.02 ms / 0.10 ms | ~26× |
| per-building value math (control, untouched) | 9.56 ms | 8.32 ms | ~equal ✓ |

Legacy PreLoop summed ~11.7 s/turn (matches the old `turn-time-optimization` ~30%/~11 s
note); now ~0.05 s/turn. The control field (`building=`) being ~equal confirms only the
enabler derivation changed, not the valuation.

**Next:** a normal playthrough to sanity-check turn-time and AI behaviour with the index
authoritative (set-equivalent, so no behaviour change expected). Then Phase 2.

---

**Status (history):** active — Phase 1 starting
**Connects:** turn-time-optimization (CABV) · derived-data-repository (Game-level static index) · the XML data-loading-coherence rework (BoolExpr as the unified, introspectable prereq form)

## The problem in one sentence

Building/unit "can I construct/train this?" logic is spread across **~38 typed
prereq fields** evaluated inline in a 475-line `canConstructInternal`, *and* a
`BoolExpr` construct-condition — and the CABV constructible-set PreLoop pays an
**O(buildings²)** cost every turn re-deriving an enabler graph that is, in fact,
**static** (a pure function of the prereq definitions, never of game state).

## What's actually there today

### canConstruct
`CvCity::canConstructInternal` (`CvCity.cpp:2511`, ~475 lines) runs the ~38 typed
checks, stratified by three gate args:
- `bExposed` — skip checks the caller already handled
- `bTestVisible` — include "could become available" (greyed-but-shown) checks
- `bIgnoreBuildings`/`bIgnoreCost` — partial-evaluation modes

then evaluates `getConstructCondition()->evaluate(pObject)` (`CvCity.cpp:2956`).
Unit training mirrors this in `CvPlayer::canTrain` + `getTrainCondition()`.

### BoolExpr — already evaluable AND introspectable
`Sources/BoolExpr.h`. Node types: `Has`(GOM, id) / `Is` / `And` / `Or` / `Not` /
`If` / `BEqual` / `Comp`(Greater/Equal over IntExpr) / `IntegrateOr` / `Constant`.
GOM types cover tech, civic, building, bonus, terrain, feature, religion,
corporation, promotion, unit, trait, etc. Three operations matter here:
- `evaluate(pObject)` → bool (used by canConstruct)
- `evaluateChange(pObject, overrides)` → BECOMES_TRUE/FALSE/… (used by the PreLoop)
- **`getInvolvesGOM(queries)` → bool** — walks the whole tree, answers "does this
  condition reference any of these GOMs?" This is the introspection the reverse-
  index needs, and it already exists.

So `BoolExpr` is already both **evaluable** and **queryable** — it satisfies the
introspection contract (help text via `buildDisplayString`, civic validation,
Python, the enabler-graph derivation) that the typed fields satisfy ad-hoc.

### The enabler graph — derived twice, persisted nowhere
1. **Load-time boolean:** `calculateEnablesOtherBuildings`
   (`CvBuildingInfo.cpp:1342`) computes a per-building flag `EnablesOtherBuildings()`
   — "does completing X unlock *anything*?" — by scanning all buildings for
   `isPrereqInCityBuilding(X)`/`isPrereqOrBuilding(X)` and bonus prereqs against X's
   free bonuses. It throws away *which* buildings; it keeps only the bool.
2. **Per-turn re-derivation:** the CABV PreLoop (`CvCityAI.cpp:12599-12661`) does:
   ```
   for each building B:                       // O(buildings)
       if !canConstruct(B): continue
       add B to set
       if B.EnablesOtherBuildings():
           for each building C:               // O(buildings) again  → O(buildings²)
               if C not in set and !hasBuilding(C):
                   if C.isPrereqInCityBuilding(B) || C.isPrereqOrBuilding(B)
                      || C.getConstructCondition()->evaluateChange(obj, B-added) == BECOMES_TRUE:
                       if canConstructInternal(C, …, eExtraBuilding=B):
                           add C to set
   ```
   The inner loop re-discovers "B unlocks C" from scratch every time the cache
   rebuilds. That relationship is **static** — it depends only on C's prereq
   definition mentioning B, not on any game state.

The within-turn + cross-turn memoization already shipped (see
`turn-time-optimization`): the set `m_buildingsToCalculate` is computed once per
cache lifetime and invalidated by `setHasBuilding`. So this PreLoop fires on cache
rebuild (after a building completes, tech lands, etc.), not every CABV call — but
*when it fires* it is O(buildings²) per city.

## The insight

The enabler relationship "completing/【granting】 X flips C's constructibility" is a
**static reverse-index** keyed on the GOM that changed:

```
GOM key (e.g. {GOM_BUILDING, X}, {GOM_BONUS, b})  →  set<BuildingTypes> dependents
```

It can be built **once at load** by introspecting every building's prereqs +
construct-condition (the typed building/bonus prereq fields are directly readable;
`BoolExpr::getInvolvesGOM` covers the condition). Then:
- The PreLoop's inner O(buildings) re-scan becomes an **O(1) index lookup** of B's
  dependents → PreLoop drops from O(B²) to O(B × avg-dependents).
- More importantly, it makes the constructible-set **change-driven**: when the city
  gains building X / bonus b / tech t, only `index[{GOM,X}]` need re-checking,
  instead of rebuilding the whole set. That is the cross-turn retention step the
  CABV work was heading toward, and it is precisely the
  **derived-data-repository Game-level static index** (north-star §6).

The index is read-only derived data — it does **not** change any gameplay decision,
only *how fast* the same set is computed. That makes Phase 1 verifiable: the
index-driven set must equal the current PreLoop set, building-for-building.

## Why unify the prereqs (#195 proper) — and why it's separable

The typed prereqs split cleanly:
- **GOM-expressible** (has-tech / has-civic / has-building / has-bonus / has-religion
  / has-corp / game-option / …): the bulk of the 38, and *exactly* the enabler-
  relevant ones. These already have a `BoolExpr` equivalent (`Has` nodes).
- **Non-GOM** (valid building location/plot checks, population, latitude bounds,
  culture level, property min/max): not enabler relationships (nothing "unlocks"
  them except organic growth). Stay typed, or become `Comp`/IntExpr where natural.

Folding the GOM-expressible prereqs into the construct-condition collapses the bulk
of `canConstructInternal` into one evaluable+introspectable tree, and makes the
index derivation **uniform** (one introspection path instead of "typed fields +
BoolExpr"). But the index can be built from *today's* mixed representation first —
so the turn-time win does not block on the (gameplay-sensitive) unification.

## Plan

### Phase 1 — Static enabler reverse-index → change-driven set  *(turn-time win, read-only, low risk)*
1. Build `map<GOMKey, set<BuildingTypes>>` (and the unit analogue) at load, from
   typed building/bonus prereqs + `getConstructCondition()->getInvolvesGOM`. Live on
   the Game-level derived-data repository (or `CvGlobals` to start).
2. Replace the PreLoop inner O(buildings) re-scan with an index lookup of B's
   dependents; keep the `canConstructInternal(C, …, eExtraBuilding=B)` confirm.
3. **Verify equivalence:** assert/log that the index-driven set == the legacy
   PreLoop set across a real game (gate behind the existing `[PERF/cabvset]` diag).
4. Then: drive set invalidation off the index on `setHasBuilding`/bonus/tech change
   instead of full rebuild (the cross-turn retention step).

### Phase 2 — Unified prerequisite model  *(additive aggregation layer, low risk)*

Chosen over a BoolExpr-fold / `canConstruct`-rewrite: that path is multi-surface (city +
player levels), would lose the per-prereq `probabilityEverConstructable` hints, and the
turn-time win is already banked — so the remaining payoff is *introspection coherence*,
best served by a unifying interface, not by rewriting the evaluator. `canConstruct` /
`canTrain` evaluation (and its hints / gate stratification) stays untouched.

**Increment 1 — DONE (model + first consumer):**
- `Sources/ConstructRequirement.h` — `ConstructRequirement { GOMTypes eGOM;
  ConstructRequirementOp eOp (ALL/ANY/FORBID/COUNT); vector<int> aiIds; int iCount; }`:
  one introspectable prereq over a GOM type. Read-only description, not an evaluator.
- `CvBuildingInfo::getConstructRequirements()` — built at load
  (`buildConstructRequirements` in `doPostLoadCaching`) from the GOM-expressible typed
  fields: GOM_BUILDING (InCity=ALL, Or=ANY, NotInCity=FORBID, NumOf=COUNT), GOM_TECH,
  GOM_BONUS (And=ALL, Or=ANY), GOM_RELIGION, GOM_CORPORATION, GOM_CIVIC (And/Or),
  GOM_OPTION.
- The constructibility enabler index now reads building prereqs **through the model**
  (GOM_BUILDING ALL+ANY = the old InCity+Or; FORBID/COUNT correctly skipped — not
  enablers). Builds clean (Assert).

**Increment 2 — DONE (unit/train side):**
- `CvUnitInfo::getTrainRequirements()` — same model, built at load
  (`buildTrainRequirements` in `doPostLoadCaching`): GOM_BUILDING (And=ALL, Or=ANY),
  GOM_TECH, GOM_BONUS (And=ALL, Or=ANY), GOM_RELIGION, GOM_CORPORATION, GOM_CIVIC (Or).
- The unit-enabler index dimension now reads through the model — building enablers from
  GOM_BUILDING **REQUIRE_ALL only** (the CABV unit-enabler loop checks `isPrereqAndBuilding`
  only, so OR-buildings are intentionally not enablers — preserves the Phase 1 verified
  behaviour); bonus needs from GOM_BONUS (any op). Builds clean.

So **both** enabler-index dimensions (building + unit) now read through the unified model.

**Model-fidelity verification — via logging, NOT asserts.** FASSERT compiles out of
FinalRelease (only Assert/Debug/Testing define `FASSERT_ENABLE`), so a one-shot sweep
`cvInternalGlobals::logConstructRequirementFidelity()` emits `[PERF/reqmodel]` lines
(building + unit model vs typed-field comparison) through the gated `[PERF]` logging
channel, which ships in every DLL. It is fired once per session from the CABV PreLoop
(where `gPerfLogLevel` is set — it is read at game init, after the load-time index build).
**Verify:** play with `Autolog__LogLevelPerf >= 1`, confirm `Performance.log` has a
`[PERF/reqmodel] … mismatches=0` line and no `[PERF/reqmodel] MISMATCH …` lines.

**Increment 3 — DONE (building coverage completed):**
- Building model extended to GOM_TERRAIN (And/Or), GOM_FEATURE (Or), GOM_IMPROVEMENT (Or),
  GOM_HERITAGE (Or). The building model now covers every clean GOM-mappable typed prereq.
- Deliberately still NOT modelled (bespoke semantics, no consumer): vicinity / raw-vicinity
  bonus (in-vicinity, not in-city), state-religion (player-level); and the non-GOM prereqs
  (population, culture level, properties, war/power) keep their typed handling. Foundation
  for the Civilopedia/web export — these GOM types have no behavioural consumer yet.

**Increment 4 — IN PROGRESS (help-text migration, cluster by cluster):**
`CvGameTextMgr::buildBuildingRequiresString` (~570 lines) hand-enumerates every typed
prereq field. Migrating it onto the model, one cluster at a time, each visually verified.
- **Cluster 1 — DONE:** terrain (Or/And) / improvement (Or) / feature (Or) "in city
  vicinity" requirements — four near-identical hand-rolled loops → one model loop +
  `appendVicinityRequirementHelp()` (no status filtering; `IN_CITY_VICINITY` suffix kept).
- **Status-aware renderer — DONE:** `appendRequirementHelp(req, pCity)` +
  `buildRequirementItemLink(GOM,id)`. Filters unmet items via `CvGameObject::hasGOM` (the
  same oracle the construct-condition uses, so displayed status == real constructibility);
  renders unmet items as a `Requires: <links>` list (AND/OR by op). Formatting normalised to
  clickable links (old blocks mixed link / description / textkey).
- **Cluster 2 — DONE + verified ("text is fine, links work"):** prereq **Or-buildings**
  (was an active/obsolete `bValid` scan → GOM_BUILDING REQUIRE_ANY) and prereq **bonuses**
  (single AND + OR list → GOM_BONUS). Nuance dropped: all-obsolete OR-building set no longer
  special-cased.
- **Cluster 3 — DONE (awaiting verification):** **InCity-buildings** (GOM_BUILDING
  REQUIRE_ALL; all-infos scan + per-line `Requires <link>` → one joined unmet list) and
  **civics** (AND + OR). Civics keep their show-all colour-coded UX via a dedicated
  `appendCivicRequirementHelp()` (have = green, need = red) that returns met-status for the
  `RequiresActiveCivics` note — replaced ~55 lines of two colour-toggle loops.
- **Intentionally left as-is:** **tech** (`bTechChooserText` gating + the model combines
  PrereqAndTech/AndTechs so the single-vs-list distinction the chooser needs is lost),
  **religion** (a coherent group rendered as `getChar` symbols — symbol vs link is a UX call),
  **NotInCity** (FORBID, "not required" inverse wording), and the non-GOM / vicinity-bonus /
  state-religion / holy-city blocks. These are documented exceptions, not omissions.

**Increment 5+ (optional, later):** re-express `canConstruct` / `canTrain` on the model,
shadow-verified, preserving probability hints + gate stratification.

## Invariants to preserve
- The `bExposed`/`bTestVisible`/`bIgnoreBuildings` gate stratification.
- Introspectability for help text, `CvPlayer::hasValidCivics`, Python bindings,
  enabler-graph derivation.
- Lockstep/OOS safety: the index is load-derived from synced info data, identical on
  every client; no RNG, no per-machine ordering.
- No gameplay change in Phase 1 (set equivalence is the gate).
