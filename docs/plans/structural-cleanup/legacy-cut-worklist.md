# Legacy-accumulator cut worklist + corrected #430 status

> **The top of the worklist.** The grounded, exhaustive list of remaining legacy serialized accumulators to cut
> (`DEC-accumulator-cut-uniform`), ordered by cut-readiness, plus a corrected #430 status (the
> [roadmap](roadmap.md) is stale on F4/F2b/F5/F8 — refresh it from §Corrected status below).
> Grounded against live code on `json-data-migration`. **The value of this cut is legacy PURGE — it exposes cascade
> gaps and unblocks a clean perf read ([DEC-legacy-decache-poisons-perf](../../architecture/decisions.md#dec-legacy-decache-poisons-perf))
> — NOT memory reclaimed** (these are small fixed per-entity int arrays; the working-set pressure is
> turn-processing churn, [memory-footprint.md](../../reference/memory-footprint.md)). Rank by leverage, not by MB.

## Already cut (baseline — do not re-list)

CvCity wellbeing pilot (`m_iExtraBuilding*`, `m_iReligion*Happiness`, `m_paiStateReligionHappiness`,
`m_iSpecialist*`); CvCity + CvPlayer yield/commerce clusters (`m_aiBuildingCommerce`, `m_aiExtraYield`,
`m_aiFreeCityYield`, `m_aiGoldenAge*`, …); `m_iBuildingDefense` (now the cascade scalar slot); freeSpecialist
AMOUNT; **unit upkeep base** (`CvUnit::m_iUpkeep100`/`m_iExtraUpkeep100` + `CvPlayer::m_iUnitUpkeep{Civilian,Military}100`
push → Σ-over-live-units recompute cache — the F4 upkeep cut LANDED, superseding the stale
[fixed-point-conformance.md](fixed-point-conformance.md) "retires at F4, not yet" note).

---

## A — ✅ CUT (the whole section is done)

The CvPlayer wellbeing feeders (`m_iCivic{Happiness,Health}`, `m_iCivilizationHealth`, `m_iLargestCityHappiness`,
`m_iProject{Happiness,Health}`, `m_iWorld{Happiness,Health}`) and the CvCity per-commerce pool
(`m_aiCommerceHappinessPer`) are deleted, their getters re-pointed to `CvCascadeWellbeing` /
`CascadeAccumulator::commerceHappinessPer`, and every tag named in `Assets/savemigration.txt`. The exposed
accessors the sweep needed (`civicWellbeing`/`civilizationHealth`/`projectWellbeing`/`worldWellbeing`/
`largestCityWellbeing`/`commerceHappinessPer`) exist.

Two things this cut established, worth carrying to the remaining sections:

- **The accessor reduces `/100`**, because a wellbeing verdict is a DISCRETE count
  ([DEC-fixedpoint-x100](../../architecture/decisions.md#dec-fixedpoint-x100) reduce-at-the-reader). That keeps
  every AI / Python / HTTP consumer's legacy ×1 arithmetic intact — a re-point with no blast radius.
- **Audit the changer body for riders** ([save.md §6](../../specs/save.md)). `changeCommerceHappinessPer` carried an
  `AI_setAssignWorkDirty(true)` that `processBuilding` does *not* fire itself; it moved to the surviving trigger
  site. A rider dropped here is a silent citizen-reassignment bug, not a compile error.

⚠ The AI `getAdditional*` what-ifs read the `CvCivicInfo`/`CvTraitInfo` **info** getters, NOT the player
accumulator — those STAY.

---

## B — BLOCKED on a channel not yet built

### B1 — the percent-modifier-stack family
Serialized + `change*`-maintained, but the **additive percent stack** ([modifier.md §2a](../../specs/modifier.md)),
not a base magnitude. The cascade must first unify `modBuilding/modPlayer/modCapital/modBonus` into one additive
sum — **that channel is not built** — so these cannot re-point yet. (`fixed-point-conformance.md` explicitly carves
them out as a distinct, lower-priority rework.)

- **CvCity:** `m_aiYieldRateModifier`, `m_aiCommerceRateModifier`, `m_aiPowerYieldRateModifier`,
  `m_aiBonusYieldRateModifier`, `m_aiBonusCommerceRateModifier`, `m_aiBonusCommercePercentChanges`,
  `m_aiProductionToCommerceModifier`, `m_iEspionageDefenseModifier`.
- **CvPlayer:** `m_ai{Yield,Capital{Yield,Commerce},Commerce}RateModifier`, `…RateModifierfrom{Buildings,Events}`,
  `m_aiTradeYieldModifier`, the maintenance/upkeep percents (`m_iMaintenanceModifier` + the 7 sub-modifiers,
  `m_iUpkeepModifier`), `m_iCityDefenseModifier`, `m_iExtraCityDefense`, `m_iNationalEspionageDefense`.
- **CvArea:** `m_aiMaintenanceModifier`. **CvTeam:** `m_iTradeModifier`, `m_iForeignTradeModifier`,
  `m_iCorporationRevenueModifier`, `m_iCorporationMaintenanceModifier`, `m_iEnemyWarWearinessModifier`.

### B2 — per-tile / option-gated base-yield accumulators
- `m_aiSeaPlotYield` (CvPlayer.h:1970) — blocked on the `plots {IS_WATER}` empire-target deposit
  ([modifier.md §5](../../specs/modifier.md)); still legacy-live on `CvPlot::calculateYield` (`CvPlot.cpp:8467`).
- `m_aiLandmarkYield` (CvPlayer.h:1589) — rides the **#448 landmark data pass** (landmark is a ruled
  KEEP-through-migration).

---

## C — BLOCKED on the KEYED unit-combat cascade step (F4 step-3)

CvCity per-unitcombat-keyed accumulators — the `IS_<TAG>` predicate surface (F2) is now BUILT (`CASC_PRED_IS_TAG`
+ `CvCascadeTally::countUnitsWithTag`, with the curator emitting tags) and the unit-plane SCALAR channels (F4) are
cut; these remain blocked on the KEYED unit-combat cascade consumer (F4 step-3 keyed vs-class), still unbuilt:
`m_paiUnitCombatExtraStrength` (1636), `m_paiUnitCombatProductionModifier` (1710),
`m_paiUnitCombatDefenseAgainstModifier` (1711), `m_paiUnitCombatFreeExperience` (1780),
`m_paiDamageAttackingUnitCombatCount` (1725), `m_paiHealUnitCombatTypeVolume` (1726) — all CvCity.h.

## NOT cut (adversarial pass — explicit negatives)
Raw-state INPUTS the cascade FOLDS (`m_iExtra{Happiness,Health}`, `m_iLandmarkHappiness`, espionage counters,
freshwater, anger timers/percents); `m_aiBuildingCommerce100` (kept dirty recompute cache); `m_aiTradeYield` (held
input package); the rank memo caches (`m_ai*Rank`, not a magnitude channel).

## Execution order
1. **B2** (`m_aiSeaPlotYield` via the `plots {IS_WATER}` empire-target deposit; `m_aiLandmarkYield` rides #448).
2. **B1** — the §2a percent-stack unification (its own larger channel, and the biggest remaining legacy surface).
3. **C** — with the keyed F4 step-3 (F2 + the scalar F4 channels are already done).

§A is cut; the next unblocked legacy purge is B2, and B1 is gated on building the additive percent-stack channel.

---

## Corrected #430 status (roadmap-refresh input)

The [roadmap](roadmap.md) predates the 14 post-`6475a79a6` commits (all F4 unit-plane + the culture-drop). Verified
current status:

| Item | Roadmap said | Actual now | Open? |
|---|---|---|:--:|
| **F0** foundation | mostly landed; one gap = broad `markPlayerScopeAndCities` at civic/tech sites | accurate — 3 broad-mark sites remain to narrow | OPEN |
| **F1** reach green | 11 uniformity pocos dead; DllExport proxy, `enPromotionValid`, gate consumers | unchanged — accurate | OPEN |
| **F2** classification + `IS_<TAG>` | unbuilt; every rewire blocks on it | **BUILT** — `CASC_PRED_IS_TAG` predicate + `countUnitsWithTag` tally exist; the curator emits 30 tags (incl. unitcombat-derived mechanized/gunpowder/mounted). Consumer rewires now UNBLOCKED, still to do | rewires open |
| **F2b** consumer-iteration | "262 whole-database loops remain" | **mostly LANDED** — hot path done (frontier getters + 12 AI loops + `AI_chooseProduction` collapse); only non-hot residual loops open | mostly done |
| **F3** grants apply-loop | unbuilt (shadows only) | **APPLYING** — per-turn provisions, building first-build, free promotions, tech/religion/unit-promotion/game-start gold, and the city-founded/capital-changed lifecycle all apply through the machine. Group-2 sites (votes, NPC spawns, combat loot, leader level-up trait, plot bonus discovery) + the game-start remainder are open | partly built |
| **F4** unit-plane | **NOT BUILT** | **mostly BUILT** — withdrawal/firstStrike/heal/evasion/intercept/collateral-damage/capture/13 combat-percent groups/base-strength/StrengthModifier/DamageModifier/upkeep landed (UPK_*, legacy m_iExtra* removed); the keyed "vs-class" step-3 is now UNBLOCKED (unitcombat→tags landed) but its keyed cascade consumer is still to be built; MaxHP + SizeMatters + movement + espionage + bombard channels remain legacy | mostly built |
| **F5** property feed | over-applies; operate-dormancy invalidation BLOCKING | feed **DONE**; the operate-dormancy watermark is now **BUILT** — `CvPropertySolver::doTurnBandWatch()` (CvPropertySolver.cpp:487) is the crossing-detector, routing bands to `propertyBands` via `EnablerKernel::onPropertyBandHitActive` (the dead write-only `s_opDynamic` is a small dead-code cleanup left). Remaining: **(a)** the flammability `/5` data rebalance | (a) open |
| **F6** manifestation (free XP/promo) | `isApplyFreePromotionOnMove` hardcoded false | unchanged — coupled to F3 | OPEN |
| **F7** data tail | unitcombat→tags, state, corp, leaderhead, NPC | partial — **344 culture unitcombats purged**; the unitcombat→`tags` pass has LANDED (curator folds combat classes to unit tags; 30 tags emitted); state/corp/leaderhead/NPC tails still open | OPEN |
| **F8** Python/Cy wrapper | redesign contract + rewire consumers | **entirely untouched** — ~899 legacy `.def` bindings, zero cascade rewire | OPEN (unstarted) |

### F5(a) — the flammability `/5` rebalance (located; NO change made)
Owner rule: **positive fire/flammability adders `/5`; negative/reducers unchanged.** Over `Assets/Data/buildings/`:
**313 positive** `PROPERTY_FLAMMABILITY.city.flat` adders (the `/5` targets) · **64 negative** reducers (unchanged).
There is no separate `PROPERTY_FIRE` — all fire data is `PROPERTY_FLAMMABILITY`. ⚠ It flows through the generic
`PROPERTY_*` fold (`curate_building.py` has no flammability rule), so the `/5` must land as a **new curator scaling
rule (positive-flammability only) → recurate + regen** ([DEC-recurate-on-decision](../../architecture/decisions.md#dec-recurate-on-decision));
hand-editing the 313 derived JSONs is banned.

### Critical path
1. **The unitcombat→tags distillation (THE keystone) has LANDED** — the curator folds combat classes to unit
   `tags` ([unitcombat-distillation.md](unitcombat-distillation.md)), unblocking the four fronts it gated: F2
   (`IS_<TAG>` predicate — built), F4 step-3 (keyed vs-class — keyed consumer still to build), F7 (tags pass —
   done), and F4-upkeep military/civilian bucketing. The consumer rewires that ride on it remain.
2. **F3 grants apply-loop** → unblocks **F6**. Independent; parallelizable.
3. **F5:** (a) flammability `/5` is cheap; (b) the operate-dormancy watermark module is BUILT (`doTurnBandWatch`) — only the dead-`s_opDynamic` cleanup remains.
4. **F0** — narrow the 3 broad-mark sites (small).
5. **F8** — large + independent (Cy contract redesign + Python consumer rewire); parallelizable.
6. The **legacy-cut §A** above — cheap, high-purge, do alongside 1–2.
