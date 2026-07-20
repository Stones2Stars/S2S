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

## A — CUTTABLE NOW (highest leverage; cascade already computes the term)

The wellbeing verdicts are pure cascade delegates (`CvCity::happyLevel/unhappyLevel/goodHealth/badHealth` =
`CascadeAccumulator::wellbeing(this,N)`), and `CvCascadeWellbeing::assemble()` computes each term below **fresh from
deposits**. So these **CvPlayer members are dead on the served path** — their only readers are the delegating
`CvCity` getters + the `/computed` oracle rows (an oracle endpoint is not a real consumer:
[DEC-oracle-tautology](../../architecture/decisions.md#dec-oracle-tautology) — remove the endpoint *with* the member).
**These were never in the `fixed-point-conformance.md` inventory** — the pilot cut the CvCity cluster and left the
CvPlayer feeders standing. **This is the top of the list — one sweep, one shared accessor add, no blocker.**

| # | member | file:line | maintainer | cascade term (`CvCascadeWellbeing.cpp`) |
|---|---|---|---|---|
| A1 | `m_iCivicHappiness` | CvPlayer.h:1596 | `changeCivicHappiness` ← `processCivics` | `hap.iCivicNet` |
| A2 | `m_iCivicHealth` | CvPlayer.h:1882 | `changeCivicHealth` ← `processCivics` | `hea.iCivicNet` |
| A3 | `m_iCivilizationHealth` | CvPlayer.h:1603 | `changeCivilizationHealth` ← `processTrait` | `hea.iTraitNet` |
| A4 | `m_iLargestCityHappiness` | CvPlayer.h:1885 | `changeLargestCityHappiness` ← civics/trait | `hap.iLargest` |
| A5 | `m_iProjectHappiness` | CvPlayer.h:1607 | `changeProjectHappiness` | `hap.project.iGood` |
| A6 | `m_iProjectHealth` | CvPlayer.h:1605 | `changeProjectHealth` | `hea.project` |
| A7 | `m_iWorldHappiness` | CvPlayer.h:1609 | `changeWorldHappiness` ← CvTeam project | `hap.project` (world scope) |
| A8 | `m_iWorldHealth` | CvPlayer.h:1608 | `changeWorldHealth` ← CvTeam | `hea.project` |
| A9 | `m_aiCommerceHappinessPer` | CvCity.h:1753 | `changeCommerceHappinessPer` | `aiCommercePer[]` — **lower confidence**, also feeds `getCommerceHappiness`; census its readers first |

**Enabling step (once):** add four thin decomposition accessors to `CvCascadeWellbeing`
(`civicWellbeing`/`projectWellbeing`/`largestCityWellbeing`/`civilizationHealth` — identical shape to the existing
`bonusWellbeing`/`buildingWellbeing`). The terms already exist inside `assemble`; only the exposed wrapper is missing.
⚠ The AI `getAdditional*` what-ifs read the `CvCivicInfo`/`CvTraitInfo` **info** getters, NOT the player accumulator —
those STAY.

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

## C — BLOCKED on unitcombat→tags (F2) + the unit-plane (F4)

CvCity per-unitcombat-keyed accumulators — depend on the `IS_<TAG>` predicate surface (F2, unbuilt) AND the
unit-plane strength/combat channels (F4 empire apply-loops, blocked):
`m_paiUnitCombatExtraStrength` (1636), `m_paiUnitCombatProductionModifier` (1710),
`m_paiUnitCombatDefenseAgainstModifier` (1711), `m_paiUnitCombatFreeExperience` (1780),
`m_paiDamageAttackingUnitCombatCount` (1725), `m_paiHealUnitCombatTypeVolume` (1726) — all CvCity.h.

## NOT cut (adversarial pass — explicit negatives)
Raw-state INPUTS the cascade FOLDS (`m_iExtra{Happiness,Health}`, `m_iLandmarkHappiness`, espionage counters,
freshwater, anger timers/percents); `m_aiBuildingCommerce100` (kept dirty recompute cache); `m_aiTradeYield` (held
input package); the rank memo caches (`m_ai*Rank`, not a magnitude channel).

## Execution order
1. **A1–A8** — one CvPlayer wellbeing-feeder sweep + one shared accessor add. Pure win, no blocker, largest
   legacy-purge blast radius per unit effort.
2. **A9** after a `getCommerceHappiness`-consumer census.
3. **B2** (`m_aiSeaPlotYield` via `plots {IS_WATER}`; `m_aiLandmarkYield` rides #448).
4. **B1** — the §2a percent-stack unification (its own larger channel).
5. **C** — with F4/F2.

---

## Corrected #430 status (roadmap-refresh input)

The [roadmap](roadmap.md) predates the 14 post-`6475a79a6` commits (all F4 unit-plane + the culture-drop). Verified
current status:

| Item | Roadmap said | Actual now | Open? |
|---|---|---|:--:|
| **F0** foundation | mostly landed; one gap = broad `markPlayerScopeAndCities` at civic/tech sites | accurate — 3 broad-mark sites remain to narrow | OPEN |
| **F1** reach green | 11 uniformity pocos dead; DllExport proxy, `enPromotionValid`, gate consumers | unchanged — accurate | OPEN |
| **F2** classification + `IS_<TAG>` | unbuilt; every rewire blocks on it | confirmed **BLOCKED** — no unit-classification predicate exists; curator emits no `tags` | **BLOCKED** |
| **F2b** consumer-iteration | "262 whole-database loops remain" | **mostly LANDED** — hot path done (frontier getters + 12 AI loops + `AI_chooseProduction` collapse); only non-hot residual loops open | mostly done |
| **F3** grants apply-loop | unbuilt (shadows only) | confirmed — `[GRANTS]` diff-shadow only, ~30 prereq rows unbuilt | OPEN |
| **F4** unit-plane | **NOT BUILT** | **mostly BUILT** — withdrawal/firstStrike/heal/evasion/…/13 combat-percent groups/base-strength/upkeep landed; **keyed "vs-class" step-3 BLOCKED** on unitcombat→tags | mostly built |
| **F5** property feed | over-applies; operate-dormancy invalidation BLOCKING | feed **DONE**; remaining: **(a)** flammability `/5` data rebalance; **(b)** the operate-dormancy watermark **still nil** — `s_opDynamic` write-only, no crossing-detector module exists (**owner's "maybe resolved" = NO**) | OPEN |
| **F6** manifestation (free XP/promo) | `isApplyFreePromotionOnMove` hardcoded false | unchanged — coupled to F3 | OPEN |
| **F7** data tail | unitcombat→tags, state, corp, leaderhead, NPC | partial — **344 culture unitcombats purged**; tags pass still blocked; rest open | OPEN |
| **F8** Python/Cy wrapper | redesign contract + rewire consumers | **entirely untouched** — ~899 legacy `.def` bindings, zero cascade rewire | OPEN (unstarted) |

### F5(a) — the flammability `/5` rebalance (located; NO change made)
Owner rule: **positive fire/flammability adders `/5`; negative/reducers unchanged.** Over `Assets/Data/buildings/`:
**313 positive** `PROPERTY_FLAMMABILITY.city.flat` adders (the `/5` targets) · **64 negative** reducers (unchanged).
There is no separate `PROPERTY_FIRE` — all fire data is `PROPERTY_FLAMMABILITY`. ⚠ It flows through the generic
`PROPERTY_*` fold (`curate_building.py` has no flammability rule), so the `/5` must land as a **new curator scaling
rule (positive-flammability only) → recurate + regen** ([DEC-recurate-on-decision](../../architecture/decisions.md#dec-recurate-on-decision));
hand-editing the 313 derived JSONs is banned.

### Critical path
1. **The unitcombat→tags distillation is THE keystone** — one pass ([unitcombat-distillation.md](unitcombat-distillation.md))
   unblocks **four** fronts: F2 (`IS_<TAG>` predicate), F4 step-3 (keyed vs-class), F7 (tags pass), and F4-upkeep
   military/civilian bucketing. Highest leverage; do first.
2. **F3 grants apply-loop** → unblocks **F6**. Independent; parallelizable.
3. **F5:** (a) flammability `/5` is cheap; (b) the operate-dormancy watermark module is a real build (still nil).
4. **F0** — narrow the 3 broad-mark sites (small).
5. **F8** — large + independent (Cy contract redesign + Python consumer rewire); parallelizable.
6. The **legacy-cut §A** above — cheap, high-purge, do alongside 1–2.
