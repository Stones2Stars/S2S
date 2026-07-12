# The duplicate surface — every legacy↔cascade pair running today (#430)

> **What this is:** the verified INDEX of the **cut-pending legacy oracles** — every value still computed by
> BOTH the legacy engine and the cascade, which side SERVES the game, and where each pair dies
> ([code-cut-map.md](code-cut-map.md) owns the cut rows). The `*Legacy` getters are **demolition-pending
> residuals**, not a sanctioned duplication: each dies at its mechanism's **atomic cut**, and the cascade that
> replaces it is verified **live in the running game** before that cut
> ([validation.md](../../specs/validation.md) — the shadow phase is closed; what remains is live verification).
> Line numbers drift — confirm the named function/member, not the integer.

## A. The CITY MODIFIER plane — CASCADE SERVES, legacy maintains itself as the oracle

Every getter below returns the package composition (`CascadeAccumulator`, `Sources/Cascade/`); its `*Legacy`
sibling reads the still-maintained legacy accumulators (fed by the untouched `process*` apply-loops in
`CvCity.cpp`/`CvPlayer.cpp`/`CvTeam.cpp`); the nets diff them per turn. All in `Sources/Engine/CvCity.{h,cpp}`.

| # | value | serving getter (cascade) | legacy oracle | legacy state behind it | net |
|---|---|---|---|---|---|
| 1 | yield rates | `getYieldRate100` | `getYieldRate100Legacy` | `m_aiYieldRateModifier`, `m_aiExtraYield`, `getBaseYieldRate` chain | `[GETTER]` + `[MODIFIER/rate]` + `[MODIFIER/slot]` |
| 2 | commerce rates | `getCommerceRateTimes100` | `getCommerceRateTimes100Legacy` | `m_aiCommerceRate`, `m_aiCommerceRateModifier`, the building-commerce chain (incl. the recompute-from-source `m_ppiBuildingCommerceChange` + the persisted `m_aBuildingCommerceChangeEvents`) | same |
| 3 | happiness ×2 | `happyLevel` / `unhappyLevel` | `happyLevelLegacy` / `unhappyLevelLegacy` | the stored wb accumulators (`m_iBonusGood/BadHappiness`, `m_iBuildingGood/BadHappiness`, `m_paiStateReligionHappiness`, …) | `[MODIFIER/wellbeing]` |
| 4 | health ×2 | `goodHealth` / `badHealth` | `goodHealthLegacy` / `badHealthLegacy` | the health twins of #3 | same |
| 5 | GP base | `getBaseGreatPeopleRate` | `getBaseGreatPeopleRateLegacy` | `m_iBaseGreatPeopleRate` | `[MODIFIER/scalar]` + the endpoint slot twins |
| 6 | GP modifier | `getTotalGreatPeopleRateModifier` | `getTotalGreatPeopleRateModifierLegacy` | `m_iGreatPeopleRateModifier` + the player twin | same |
| 7 | building defense | `getBuildingDefense` | `getBuildingDefenseLegacy` | `m_iBuildingDefense` (carries the documented drift) | same |
| 8 | maintenance mod | `getEffectiveMaintenanceModifier` | `getEffectiveMaintenanceModifierLegacy` | the city/player/area maintenance accumulators (the area one is pure phantom) | same |
| 9 | buildRate ×3 | `getProductionModifier(Unit/Building/Project)` | `getProductionModifierLegacy` ×3 | the city+player unit/building/combat/domain/military/space production-modifier accumulators | the endpoint `buildRateCasc/Leg` + parts |

**Row 10 FLIPPED 2026-07-04 ("rip the bandaid") — the plane is now WHOLLY cascade-serving:**

| # | value | serving getter (cascade) | legacy oracle | legacy state behind it | net |
|---|---|---|---|---|---|
| 10 | trade routes | `getTradeRoutes` (`scTradeRoutes` + the live vote-store/INITIAL folds + the project world grants; legacy clamp at the getter) | `getTradeRoutesLegacy` | `CvGame::m_iTradeRoutes` (clean vote/WB store — folds as a raw input), `CvPlayer::m_iTradeRoutes` (fully derivable), `CvCity::m_iExtraTradeRoutes` (MIXED building+WB — its store split lands at the demolition; the NAMED residue class) | `[MODIFIER/scalar]` + the endpoint `tradeRoutesCasc/Leg/Slot` twins |

**Dual state under the plane:**

| # | state | cascade side | legacy side | net |
|---|---|---|---|---|
| 11 | building ACTIVE/dormant | `m_operatingBuildings` (the operate/provides fixpoint; every cascade fill reads it) | `setDisabledBuilding` event-state (still drives the engine's own processing; carries the accepted dropped-event staleness) | `[MODIFIER/dorm]` attribution lines |

## B. The ENABLER plane — ⚡ FLIPPED 2026-07-04 ("flip it all"): CASCADE SERVES, legacy maintains as the oracle

The gates serve the cascade frontier, CACHED on the package substrate (`CPK_FRONTIER` city sets:
buildable/trainable/creatable/maintainable; `PSC_FRONTIER` player sets: researchable/civics/hurries + the
canBuild rem-set + the promotion tech halves) — **ensure-on-read (the operating buildings idiom, deliberately not the
rates' bare fetch: gate reads are decision-time and legacy chains builds within a turn)**, filled by the
harness-proven cascade calls (`BuildingCascade::buildable` / `UnitCascade::trainable` /
`TechCascade::available` / the kernel gateSets). The flipped bodies (default shapes; what-if/visible params

+ pre-init ride Legacy): `CvCity::canConstruct/canTrain/canCreate/canMaintain`,
`CvPlayer::canResearch/canDoCivics/canHurry/canFoundReligion` + the `canBuild` UNLOCK half,
`CvUnit::isPromotionValid` (the composite: frontier half over the bespoke `isPromotionValidLegacy(...,true)`
ride). Every gate keeps its intact `can*Legacy` oracle; `[ENABLER/shadow]` now diffs SERVING-vs-oracle. The
legacy gate caches (`m_bCanConstruct*`, the canTrain cache) serve the Legacy path only; the
`CvCityAI::CalculateAllBuildingValues` PreLoop rides legacy until the enabler CUT (the deletion still waits
for live verification + the standing gates).

## C. The THIRD surface — the oracle calculators (net-sampled only)

`YieldRate` / `CommerceCalc` / `CascadeWellbeing::compute` / `CascadeScalarChannels` (`Sources/Cascade/`):
from-scratch derivations the `[SLOT]`-class nets sample (capped per turn). Not game-serving; they die at the
cut (the one-generic-assembler consolidation is the parked end-state, scope-packages.md).

## D. NOT duplicated (already single-surface)

+ **Capabilities** — CUT (the 22 `CvTeam` getters run `CascadeCapabilities`; 21 counters deleted; sliders +
  `hasLanguage` cut in wave 2). ✅ Its cache converged onto the Set protocol 2026-07-05 (`CascadeTeamCaps`
  owner-side on `CvTeam`, the scope-packages §3b census row).
+ **Plot yields** — the `CvPlot` cache IS the one source (both the cascade combine and legacy pull it; the
  push-maintained `m_aiBaseYieldRate` member is dead).
+ **The tally** — a read-only accessor over object-owned counts by design (a duplicate would be tautological).
+ **readJson/InfoRepo static data** — parallel to the XML infos by design until the final data flip (the XML
  stays authoritative for the EXE-bound accessor surface; the atomic last step).

## E. The UNIT plane — LEGACY ONLY (no duplication yet)

The unit stat stack (the ~91 `CvUnit::changeExtra*` setters) has no cascade side yet — it needs the
`unitInput` endpoint + the unit families on the one engine, then its own live-verification pass and cut.

## F. The save surface until the cut

The legacy accumulators still SERIALIZE (saves unchanged; the packages never serialize). At each cut, every
deleted serialized member retires two-stage: drop the write + a named `WRAPPER_SKIP_ELEMENT` on the read +
the `savemigration.txt` ledger entry ([engine.md](../../reference/engine.md) §Save/load — the capabilities
lesson: a deleted read DESYNCS old saves).

## The cost of running both surfaces (measured)

Both bookkeepings run per mutation (legacy incremental pushes + cascade marks); memory holds both states
(~2.5GB on the reference save, with headroom); the oracles cost only net samples; saves are unchanged. The
duplication closes plane-by-plane per [cutover.md](cutover.md), executed from [code-cut-map.md](code-cut-map.md).
