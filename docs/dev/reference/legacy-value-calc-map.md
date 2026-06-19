# Legacy value-calculation map (#430) — the DESTROY-pass map + the calc-emulator dump spec

> **Reference (how the code computes value TODAY), built from a 2026-06-19 parallel agent sweep.** Two jobs:
> (1) the **DESTROY-pass map** — you cannot delete a legacy calc you have not mapped (Orwell bar for calcs,
> [`../plans/cascade-engine-430.md`](../plans/cascade-engine-430.md) §4; [`../plans/modifier-cascade-spec.md`](../plans/modifier-cascade-spec.md) §9);
> (2) the **per-channel dump spec** for [`../plans/calc-emulator-spec.md`](../plans/calc-emulator-spec.md) — what `/diagnostic/cityInput`
> (and `unitInput`) must emit so the emulator reproduces each realized value EXACTLY (the fidelity credential).
> **Line numbers DRIFT — confirm against the named function** (and verify getter VISIBILITY before calling from `CvHttpServer.cpp`).

Channels in demolition order (calc-emulator §4). Each: the realized getter + formula, the dump fields for fidelity, the x1/x100 + clamp gotchas.

---

## 1. City YIELDS — `CvCity::getYieldRate100` ✅ BUILT (cityInput)

`getYieldRate100(y) = min(CITY_MAX_YIELD_RATE, max(100, (getBaseYieldRate + getSpecialistYieldTotal) * getBaseYieldRateModifier + 100 * getExtraYield))` (CvCity.cpp ~11246). `getBaseYieldRateModifier` = full % (`100 + Σ%`); `getExtraYield` = **x1 TRUNCATED** flat-outside (`getExtraYield100/100` — sub-100 precision lost before ×100). Result x100.
**Dump:** base, specialist (`getSpecialistYieldTotal`), modifier (`getBaseYieldRateModifier`), extraYield (x1), extraYield100, legacy100, cap. **DONE + verified live** (London 3/3).

## 2. COMMERCE split — `CvCity::getCommerceRateTimes100`

Two-stage; result x100. `getCommerceRateAtSliderPercent(eC, slider)` (CvCity.cpp ~11953):
```
if isDisorder(): 0
iRate  = min(CITY_MAX_YIELD_RATE100, getYieldRate100(YIELD_COMMERCE))
iExtra = min(CITY_MAX_YIELD_RATE100, getBaseCommerceRateExtra(eC))
iRate  = iRate * slider / 100 + iExtra
if iRate < CITY_MAX_YIELD_RATE:
    iRate = (iRate>0) ? iRate*getTotalCommerceRateModifier(eC)/100 : iRate*100/getTotalCommerceRateModifier(eC)
    iRate += getYieldRate(YIELD_PRODUCTION) * getProductionToCommerceModifier(eC)
if iRate<0 and (eC==CULTURE or RESEARCH): return 0
if iRate < MIN_TOL_FALSE_ACCUMULATE (-9999): return CITY_MAX_YIELD_RATE
return min(CITY_MAX_YIELD_RATE, iRate)
```
- `getBaseCommerceRateExtra` (x100) = 100×(specialist + extraSpecialist) + 100×religion + 100×corp + `getBuildingCommerce100` + 100×player.getExtraCommerce100... + minted (gold) + 100×goldenAge.
- `getTotalCommerceRateModifier` (base 100) sums bonus + building + player-from-buildings + event + player (− event − from-buildings, **double-count subtraction**) + capital; `max(1, ·)`.
- Gold is RESIDUAL: `getCommerceFromPercent` for gold = yield×(100 − Σother-sliders)/100.
**Dump (per commerce gold/research/culture/espionage):** slider (`player.getCommercePercent`), baseExtra100 (`getBaseCommerceRateExtra`), totalModifier (`getTotalCommerceRateModifier`), prodToCommerce (`getProductionToCommerceModifier`), realized100 (`getCommerceRateTimes100`). City-level once: yieldCommerce100 (`getYieldRate100(YIELD_COMMERCE)`), prodRate (`getYieldRate(YIELD_PRODUCTION)`), isDisorder, consts CITY_MAX_YIELD_RATE100 + MIN_TOL_FALSE_ACCUMULATE.
**⚠ verify visibility** of getBaseCommerceRateExtra / getTotalCommerceRateModifier / getProductionToCommerceModifier (may be private → add an accessor or dump a public twin).

## 3. HEALTH + HAPPINESS — good/bad signed-split

`goodHealth()` (CvCity.cpp ~5831) = Σ `max(0, source)` over: freshWater, feature, bonus, totalGoodBuildingHealth, extraHealth, handicap, improvement/100, specialist/100, corporation, extraTechHealth, player.{extra,civic,civilization,world,project}Health. `badHealth()` (~5858) = `unhealthyPopulation − Σ min(0, source)` (same sources, negative parts) − espionageHealthCounter. `healthRate = min(0, good − bad)`.
`happyLevel()` (~5689) = Σ `max(0, source)` over ~22 sources (revSuccess, largestCity, military, stateReligion, building good, feature/bonus good, religion good, commerce, area/player building, extra, handicap, vassal, civic, specialist/100, world, project, corporation, celebrity, techHappiness, +temp). `unhappyLevel()` (~5606) = `(Σ anger% × (pop+extra) / PERCENT_ANGER_DIVISOR) − Σ min(0, good source) + Σ bad source`; gated 0 by `isNoUnhappiness`/no-capital-unhappiness. anger% = overcrowding+noMilitary+culture+religion+hurry+conscript+defy+warWeariness+revRequest+revIndex+Σcivic.
**Gotchas:** specialist/improvement health & happiness are **/100**; espionage counters clamped to 8; flags `isNoUnhappiness`/`isNoUnhealthyPopulation`/`isBuildingOnlyHealthy` zero-out; per-pop via `calculatePopulationHealth/Happiness`.
**Dump:** realized `goodHealth`/`badHealth`/`healthRate` + `happyLevel`/`unhappyLevel`/`angryPopulation`; for fidelity-reproduction dump the component buckets (building good/bad, bonus, feature, specialist/100, extra, per-pop, civic/trait/player, corporation, tech) + the anger%-sum + pop + the gate flags. (Many components — itemize the buckets the maps list.)

## 4. City DEFENSE — `CvCity::getDefenseModifier`

`getTotalDefense(bIgnoreBuilding)` (CvCity.cpp ~10198) = `max(bIgnoreBuilding?0:getBuildingDefense(), getNaturalDefense()) + player.getCityDefenseModifier() + calculateBonusDefense()`.
`getDefenseModifier(bIgnoreBuilding)` (~10204) = `isOccupation() ? 0 : max(getExtraMinDefense(), getTotalDefense() * (MAX_CITY_DEFENSE_DAMAGE − getDefenseDamage()) / MAX_CITY_DEFENSE_DAMAGE)`.
- `getBuildingDefense` = `m_iBuildingDefense` (aggregate of all buildings' defenseModifier, not per-building). `getNaturalDefense` = culture-level cityDefenseModifier. `calculateBonusDefense` = Σ over had bonuses. Flat percents, integer division in the damage decay; floored at `getExtraMinDefense`. (`getMinimumDefenseLevel` is a SEPARATE production-gate floor, only under `GAMEOPTION_COMBAT_REALISTIC_SIEGE`.)
**Dump:** buildingDefense, naturalDefense, cityDefenseModifier (player), bonusDefense (`calculateBonusDefense`), defenseDamage, MAX_CITY_DEFENSE_DAMAGE, extraMinDefense, isOccupation, realized getTotalDefense + getDefenseModifier.

## 5. MAINTENANCE / UPKEEP — cost-style via `getModifiedIntValue`

`getModifiedIntValue(v, mod)` (CvGameCoreDLL.cpp:689) = `mod>0 ? v*(100+mod)/100 : mod<0 ? v*100/(100−mod) : v` — the shared **cost-asymmetric** combiner.
CITY maintenance `getMaintenanceTimes100` (CvCity.cpp ~7579, x100): `era.getInitialCityMaintenancePercent() + getModifiedIntValue(calculateBaseMaintenanceTimes100(), getEffectiveMaintenanceModifier())` (skipped if disorder/WeLoveTheKing/pop 0). Base = Σ building+distance+numCities+colony+corporation (each `…Times100`). EffectiveModifier = city `getMaintenanceModifier` + player + `area()->getTotalAreaMaintenanceModifier` (+ connected-to-capital). Caps: numCities ≤ 2,000,000; colony capped; rebels ×50%.
CIVIC upkeep `getSingleCivicUpkeep`/`getCivicUpkeep` (CvPlayer.cpp ~14219): `(max(0,(pop+OFFSET)*UpkeepInfo.populationPercent/100) + max(0,(cities+OFFSET)*cityPercent/100))` → getModifiedIntValue(upkeepModifier) → ×handicap.civicUpkeep% → (AI) ×AI mods; `max(1,·)`; rebels halve total.
UNIT upkeep `getFinalUnitUpkeep` (CvPlayer.cpp ~10327): `(civilianNet + militaryNet) × handicap.unitUpkeep%/100 × (AI) aiUnitUpkeep%/100 × (100+aiPerEra×era)/100`. Per-unit `calcUpkeep100` (CvUnit.cpp ~15798, x100): `(100×baseUpkeep + extraUpkeep100)` → getModifiedIntValue(upkeepModifier) → getModifiedIntValue(upkeepMultiplierSM).
**Dump:** city — eraInitialPercent, the 5 base components (×100), effectiveMaintenanceModifier, realized getMaintenanceTimes100, disorder/WLTK flags. player — civicUpkeep, finalUnitUpkeep (+ the handicap/AI/era mults as inputs).

## 6. UNIT-plane stats — `CvUnit::maxCombatStr` / `currCombatStr` (separate `unitInput` endpoint)

`baseCombatStrPreCheck` (CvUnit.cpp ~11341) = `(m_iBaseCombat + getExtraStrength()) × (100 + getExtraStrengthModifier())/100`. `maxCombatStr(plot,attacker)` (~11465, ~730 lines) = `baseCombatStr × modifier` where modifier = Σ ~40 situational sources (getExtraCombatPercent, plot/city/hills/feature/terrain, vs-unitCombat/domain/animal/religious/size, surrounded, attack/defense mods); `max(1, ·)`; SM divides by 100. `currCombatStr = maxCombatStr × getHP()/getMaxHP()`.
**⚠ CRITICAL (the build constraint):** CvUnit exposes **AGGREGATE `getExtra*` ONLY — no per-source getter.** Per-source attribution = iterate the unit's promotions (`getPromotionKeyedInfo`/`isHasPromotion`) + unitcombats (`getUnitCombatKeyedInfo`/`isHasUnitCombat`) and sum each from `CvPromotionInfo`/`CvUnitCombatInfo` externally. The dump should emit the aggregate `getExtra*` set + the unit's promotion/unitcombat lists (the emulator attributes per-source from the Info JSON).
**Stat vocabulary (the dumpable `getExtra*` set, CvUnit.h):** strength group (combatPercent, strength, strengthModifier, city/hills attack&defense, vsBarbs, religious, attack/defenseCombatModifier, damage, unnerve/enclose/lunge/dynamicDefense, maxHP), withdrawal, firstStrikes/chanceFirstStrikes, collateral, bombardRate, air(range/intercept/evasion), moves/moveDiscount, enemy/neutral/friendlyHeal, visibilityRange, workMod-per-build, capture(prob/resist), vs-keyed terrain/feature/unitCombat/domain/flanking. **Aggregate-fidelity first; per-source = the promotion/unitcombat attribution pass.**

---

## 7. DUPLICATE / REDUNDANT computation — the DESTROY-pass dedup map

What the cascade UNIFIES (delete N paths → one accumulator). From the 2026-06-19 sweep (cost/combat half landed; economic half pending).

### Cost / combat (verified 2026-06-19)
- **`getModifiedIntValue` cost-combiner — ~23 call sites** (CvCity.cpp production-cost 3621/3626/3631, growth 6005-6008, hurry 6065-6068, maintenance 7616 + distance/numCities/scope deltas 7473/7494/7516/7540/7564/7566, corp 7862; CvPlayer.cpp civic-upkeep 14242/14254, free-unit 10220/10225, research 8214/15540/15546, warWeariness 10945-10959, production fallback 17305/17577). The cascade routes ALL cost mods through ONE combiner site.
- **Maintenance modifier triple-sum (city + player + area)** — `getEffectiveMaintenanceModifier` (CvCity.cpp:7590) sums city `getMaintenanceModifier` + player + `area()->getTotalAreaMaintenanceModifier`; **re-summed independently for UI** at `CvDLLWidgetData.cpp:5081` (a scope-mismatch duplicate). Resolve scope order ONCE/turn.
- **Unit extra-stat DUAL-FEED (8 stats)** — strength, strengthModifier, maxHP, attackCombatMod, defenseCombatMod, upkeep100, bombardRate, cargo are each fed from BOTH `processPromotion` (CvUnit.cpp ~18678+) AND `processUnitCombat` (~18283+) into the SAME `m_iExtra*` member. One unified deposit, not two pipelines.
- **Commander/commodore re-walk** — `getExtraAttackCombatModifier`/`getExtraDefenseCombatModifier` (CvUnit.cpp ~15606-15655) re-traverse commander+commodore pointers and re-sum on EVERY call (not cached).

### Economic (yields/commerce/health/happiness) — verified 2026-06-19
- **Commerce-modifier ADD-THEN-SUBTRACT dedup** — in `getTotalCommerceRateModifier` (CvCity.cpp ~12008-12021), `CommerceRateModifierfromEvents` and `…fromBuildings` are ADDED then SUBTRACTED (they're folded into the player's generic `getCommerceRateModifier` AND tracked separately for UI). The cascade keeps ONE accumulator, no reverse-subtraction.
- **Parallel city/area/player accumulators** — building **good/bad health** (city `getBuildingGoodHealth`:8294 + `area()`:659 + player:10798, summed in `goodHealth` ~5809) and building **happiness** (city:8449 + area:701 + player:10867, summed in happy/unhappy ~5644/5705). Three scopes summed at read time → the cascade resolves scope roll-up once.
- **x1 / x100 twins** — `getYieldRate`÷`getYieldRate100`, `getCommerceRate`÷`getCommerceRateTimes100`, `getBaseCommerceRate`÷`…Times100`, `getExtraYield`÷`getExtraYield100`, `getSpecialistCommerce`÷`m_aiSpecialistCommerce100`: the x1 is always `x100/100` (a derived twin, not a separate value). Cascade stores once.
- **`getTotalCommerceRateModifier` is the central hub** (CvCity.cpp ~11995, cached w/ dirty flag) assembling bonus+building+player-from-buildings+event+player(−dedup)+capital — the single point the commerce-modifier cascade replaces.
- **No-cache recompute HOTSPOTS** (recomputed every call, multi-source, called per turn by UI/AI): `goodHealth`/`badHealth` (~13 sources each), `happyLevel`/`unhappyLevel` (~20-25 sources each), `getBaseYieldRateModifier`, `getCommerceFromPercent`, `getBaseCommerceRateExtra`, and O(n) loops `getImprovement{Good,Bad}Health`/`getSpecialist{Good,Bad}Health`/`getSpecialist{,Un}Happiness`. The cascade's O(1) summed accumulator is the direct replacement (perf win + dedup).

---

## 8. THE COMPLETE FAMILY INVENTORY — "all the things that modify all the places" (checklist)

The definitive universe of modifier FAMILIES (the modifiable variables / "places"), from a 2026-06-19 inventory
sweep of data-model-spec §1.1 + modifier-spec §1.1/§2/§5 + the `migration-renames` per-entity family columns.
Status = the EMULATOR's coverage (dump + fidelity guard), NOT the cascade engine's. The "things that modify" each
= the source classes (buildings/civics/techs/traits/bonuses/religions/corps/specialists/events/handicap/era/cultureLevel/…).

- **✅ LIVE (dump + guard verified):** `food`, `production`, `commerce` (city yields).
- **◐ DUMP built, guard pending:** the commerce split — `gold`, `research`, `culture`, `espionage` (commerce-derived).
- **📋 MAPPED (§1-5 above; dump + guard pending):** `health`, `happiness` (good/bad split), `defense` (amount/min/bombard/…), `maintenance`, `upkeep`.
- **📋 MAPPED (§6; needs the `unitInput` endpoint, aggregate-only):** unit-plane — `strength` (+ all members), `withdrawal`, `firstStrike`, `bombard`, `collateral`, `air`, `heal`, `capture`, and partials `movement`/`experience`/`workRate`/`cargo`/`vision`/`espionage`(unit).
- **✅ MAPPED (§9, 2026-06-19 wave-2):** each `PROPERTY_*` (solver, §9.1), `revolution` (Python-authoritative — only the C++ anger step is reproducible, §9.2), `growth`+`foodKept` (§9.3), `inflation`/`hurry`+`hurryAnger`/`freeExperience` (§9.4), `buildRate`/`greatPeopleRate`/`tradeRoutes` (§9.5). **NB `culture` RATE = `COMMERCE_CULTURE`** (already captured via commerce); only plot-culture *spread* is extra (spatial → #429).
- **✅ MAPPED (§10, wave-3) — with corrections:**
  - plot-substrate: `movement` (plot terrain/feature base + team/route), `cultureDistance` (spatial), `buildTime`, `vision` — §10.1. (Plot YIELD base decomposes the §1 yield `base`.)
  - cost/duration scaling: `costs`/`buildCost`/`techCost`, `durations`, `perEra`, `missionYieldMultiplier` — §10.2, all via the `getModifiedIntValue` hub (§7 dedup).
  - building-level: `cityCapture`, `occupationTime`, `espionageDefense`, `populationGrowthRate` (FLOAT log-space), `healing` — §10.3. **`pillageGold` is an ORPHANED/dead building field** (→ drop).
  - **CORRECTIONS:** `celebrity happiness` and `byCargo` **do NOT exist** in the code (vestigial spec entries → drop); `byOccupant` = military happiness only. `spawnRate` is a **stochastic per-plot event**, not a per-turn value-channel. `stateReligion` is a predicate gate on existing families (6 sub-modifiers), not its own channel. Inert/edge unit flags `poison`/`revoltProtection`/`survivor` stay out.
- **⛔ NOT modifier families (out of the emulator):** `grants`/`enables`/`obsoletes`/`replaces`/`requires`/`allowed`/`identity`/`ui`/`world`/`sound`/`cost`/`ai`; one-shot pulses (`goldenAge`, population/revolution bursts) live in `grants`.

*(So the wave-1 6 channels + wave-2 in-flight 10 cover the per-turn city/unit value calcs; the "still unmapped" bucket — plot-substrate, cost/duration scalers, building-level + crossover families — is the next minion wave to reach the TRUE complete picture.)*

## 9. Wave-2 channel maps (2026-06-19 minion wave)

### 9.1 PROPERTY (each `PROPERTY_*`) — a STATEFUL SOLVER, not a sum

Realized value getter: `CvProperties::getValueByProperty(eProp)` (CvProperties.cpp ~101) — **RAW INT, no x100** (a CRIME value of 50 is 50). Per-turn value = `CvPropertySolver::gatherAndSolve` (CvPropertySolver.cpp ~421) — **3 phases, each predict→computePredict→correct→apply**: **(1) Propagators** (cross-OBJECT spread/gather/diffuse), **(2) Interactions** (cross-PROPERTY convert-constant/convert-percent/inhibited-growth), **(3) Sources**.
- **Sources (the yield-like deposits):** `CONSTANT` (`iAmountPerTurn`), `CONSTANT_LIMITED` (cap to `iLimit`), `DECAY` (`-(iPercent * max(|v| - iNoDecayAmount, 0)) / 100`, gated off below the no-decay threshold), `ATTRIBUTE_CONSTANT` (`object.getAttribute(eAttr) * iAmountPerTurn`, e.g. ×population). Authored on `CvPropertyInfo::m_PropertyManipulators` (+ per-object manipulators), NOT on the building directly.
- **`targetLevel` / `operationalRangeMin/Max`** are AI-need + UI/heuristic only — **NOT solver clamps** (no per-turn hard bound; the predict/correct split is what converges).
- **Effect bands** (`PropertyBuilding {iMinValue,iMaxValue,eBuilding}`) gate the effect-buildings each turn (`CvCity::checkPropertyBuildings` ~1507) — the dormancy the cascade models via `requires.operate` (data-model §4.2b).
- **⚠ EMULATOR implication:** "first-class yield" is true for the DEPOSIT shape; the per-turn VALUE is the solver. **Propagators = the #429-deferred SPATIAL leakage** — so the emulator's property channel reproduces the **sources + interactions** delta (the non-spatial part) and flags propagators as #429 (out of the containment model). It is NOT a bare per-source sum like yields; it's `value + Σ(source deltas via predict/correct) (+ interactions)`, spatial excluded.

### 9.2 REVOLUTION — PYTHON-authoritative index; only the C++ anger step is reproducible

**The index is computed entirely in PYTHON** (`Assets/Python/Revolution/Gameready/Revolution.py` `updateLocalRevIndices` ~974): per-turn Δ = `gameSpeedMod × revIdxModifier × Σ(localEffects) + revIdxOffset + feedbackDecay`, over ~15 grievances (happiness, distance-to-capital, colony, connectivity, religion, cultureRate, nationality, health, garrison, spirit, size, starvation, occupation, civics/traits/buildings via `RevUtils`, difficulty) — using `pow()` + float math. **C++ is READ-ONLY:** `m_iRevolutionIndex` is set by Python (`setRevolutionIndex`/`changeRevolutionIndex`, CvCity.cpp ~956-969); `getRevolutionIndex`/`getLocalRevIndex` expose it.
- **The one C++-reproducible part — anger conversion** (CvCity.cpp:5509): `getRevIndexPercentAnger = min(40, (125 + min(getLocalRevIndex()*5, 100)) * (getRevolutionIndex() - 325) / 7500)`, 0 below index 325; feeds `unhappyLevel` (already an input in §3).
- **⚠ EMULATOR / OOS:** the INDEX calc is Python-authoritative + float/`pow` → **NOT C++-reproducible, NOT OOS-deterministic** (the pending Python→C++ port, `migration-renames` §Civic). The `revolution` channel reproduces ONLY the C++ anger step (dump `revolutionIndex` + `localRevIndex` → assert `getRevIndexPercentAnger`); the index computation is flagged Python-side / deferred. `m_iRevolutionIndex` saves as a plain int.

### 9.3 GROWTH + foodKept — cost-style, reproducible

`foodDifference()` (CvCity.cpp:5980) = `getYieldRate(YIELD_FOOD) - foodConsumption()` (disorder→0; foodProduction city→`min(0,·)`; pop1/food0→`max(0,·)`). `foodConsumption` = `getFoodConsumedByPopulation - (angryPop if noAngry) - healthRate + foodWastage`.
`growthThreshold()` (CvCity.cpp:6003) = `getModifiedIntValue(player.getGrowthThreshold(pop), city.getPopulationgrowthratepercentage() + player.getPopulationgrowthratepercentage())`, `×0.5` if hominid, `max(1,·)`. `player.getGrowthThreshold` (CvPlayer.cpp:24435) = `BASE_CITY_GROWTH_THRESHOLD + (pop-1)*CITY_GROWTH_MULTIPLIER`, `×gamespeed% ×era.growthPercent% ×(AI handicap) ×(goldenAge less-food)`.
`foodKept`: `getFoodKeptPercent` clamped **[0,99]** (per-source building `getFoodKept`); stored `m_iFoodKept` capped at `growthThreshold × pct/100`, refund-on-growth.
- **EMULATOR:** `growth` (threshold + foodDifference) + `foodKept` — reproducible from base-threshold + growth% + food produced/consumed. Uses `getModifiedIntValue` + gamespeed/era/handicap/goldenAge scalers (overlaps §7 cost/duration dedup).

### 9.4 INFLATION / HURRY / FREE-XP / CULTURE

- **inflation** (CvPlayer.cpp:7965/8008): `getInflationMod10000` = base `100×getHurriedCount() × handicap.inflationPercent/100`, modified by `(m_iInflationModifier + civic+project+tech+building inflation − 100×isRebel)` via `getModifiedIntValue` (+AI handicap), returns `10000 + perTurn`. `getInflationCost = preInflated × mod/10000 − preInflated`; `preInflatedCosts` = treasuryUpkeep + totalMaintenance + civicUpkeep + finalUnitUpkeep + unitSupply + corpMaint. **Anarchy→0. x10000 scale.** Per-source: civic/project/tech/building inflation getters.
- **hurry** (CvCity.cpp:6094/6117): gold = `hurryCost × hurry.goldPerProduction` (min 1); pop = `1+(cost-1)/prodPerPop` (min 1). **Anger** `getHurryPercentAnger` (CvCity.cpp:5448, timer-based, feeds `unhappyLevel` §3): `1+(1+(timer-1)/flatHurryAngerLength)×HURRY_POP_ANGER×PERCENT_ANGER_DIVISOR/pop`; `flatHurryAngerLength = HURRY_ANGER_DIVISOR ×gamespeed% ×(100+getHurryAngerModifier)%`. Per-source: building `m_iHurryAngerModifier` + player `getNationalHurryAngerModifier`.
- **freeExperience** (CvCity.cpp:3187, at unit-build): `getFreeExperience(city) + player.getFreeExperience()` + (if `canAcquireExperience`) `getSpecialistFreeExperience` + `getUnitCombatFreeExperience(combat+sub)` (city+player) + `getDomainFreeExperience` (≥0). Per-source: building/civic/trait `changeFreeExperience`.
- **culture** — ⚠ city culture RATE **IS** `getCommerceRateTimes100(COMMERCE_CULTURE)` (`doCulture`, CvCity.cpp:16386) → **already captured via commerce** (no separate channel needed). The EXTRA is **plot-culture SPREAD** (`doPlotCulture` distance-dropoff, CvCity.cpp:16393) = **SPATIAL (#429-adjacent)** + plot decay — out of the containment model, like property propagators. Storage x100.

### 9.5 buildRate / greatPeopleRate / tradeRoutes

- **buildRate** (`getProductionModifier(item)`, CvCity.cpp:3867/3911/3940): summed signed-% from player+city `unit/building/project/domain/unitCombat/military/space/stateReligion/bonus` mods. Applied as a **DISCOUNT**: `effectiveCost = max(1, getModifiedIntValue(player.getProductionNeeded(item), −modifier))` ≈ `base×(100−mod)/100`. x1 signed %.
- **greatPeopleRate** (`getGreatPeopleRate`, CvCity.cpp:7153) = `getBaseGreatPeopleRate × getTotalGreatPeopleRateModifier / 100`; disorder→0. base = `max(0, m_iBaseGreatPeopleRate) + player.getNationalGreatPeopleRate`; modifier = `100 + city + player + (stateReligion) + (goldenAge)`, `max(0,·)`. Threshold = `player.greatPeopleThresholdNonMilitary` = `GREAT_PEOPLE_THRESHOLD ×era.greatPeoplePercent ×(GPThresholdMod) ×gamespeed /10000`, `max(1)`. Per-source: building `getGreatPeopleRateChange`/`getGreatPeopleRateModifier`(+global), specialist `getGreatPeopleRateChange`.
- **tradeRoutes** — count `getTradeRoutes` (CvCity.cpp:15347) = `game + player + (coastal) + extra`, clamped `[0, getMaxTradeRoutes]` (`MAX_TRADE_ROUTES + player adj`). Per-route profit `calculateTradeProfitTimes100` = `getBaseTradeProfit × totalTradeModifier / 100` (x100, base floored at 100); yield `calculateTradeYield` = `profit × player.getTradeYieldModifier(yield) / 100`. `totalTradeModifier` = `100 + route + pop + team + capital + overseas + foreign + peace + sharedCivics`.

## 10. Wave-3 channel maps — plot-substrate, scalers, building-level, crossovers (2026-06-19)

### 10.1 PLOT-SUBSTRATE (feeds the captured city-yield `base`)
- **Plot yield** (`CvPlot::getYield`/`calculateYield` ~8148/8320): `calculateNatureYield + extraYield + cityChange + popChange + terrainYieldChange + seaPlotYield + workingCityYieldChange + landmark + extra/lessYieldThreshold + goldenAge + improvementYieldChange + routeYieldChange`, `max(0,·)`; city plots `max(getMinCity,·)`. `calculateNatureYield` = `getBaseYield (terrain + feature + river + hills/peak) + bonus.getYieldChange`. **The city SUMS worked-plot yields → `m_aiBaseYieldRate` → `getBaseYieldRate`** — i.e. the §1 yield channel's `base` decomposes HERE.
- **movementCost** (`CvPlot::movementCost` ~4487): route-path (min of route costs) OR `terrain + feature + hills + riverCrossing + peak − extraMoveDiscount`, ×MOVE_DENOMINATOR, doubleMove ÷2/÷4, `max(90,·)`/min.
- **cultureDistance** (`CvCity::cultureDistance` ~6165): euclidean OR (REALISTIC_SPREAD) per-plot terrain/feature/route/bonus/hills culture-distance + shortest-neighbor path → **SPATIAL** (#429-adjacent).
- **buildTime** (`CvPlot::getBuildTime` ~3599): `build.getTime + featureTime`, ×peak%, − existing-route time, ×`terrain.buildModifier` ×gamespeed.hammerCostPercent ×era.buildPercent.
- **vision**: `seeFromLevel` = `improvement.seeFrom + (!water ? 1 + elevation : extraWaterSeeFrom)`; `seeThroughLevel` = `(!water ? 1+elev : 0) + feature.seeThroughChange`.

### 10.2 COST / DURATION SCALERS — all route through `getModifiedIntValue` (the §7 hub)
- **production cost** `getProductionNeeded(unit/building/project)` (CvPlayer.cpp ~7008+): `base×100 ×gamespeed.hammerCostPercent ×era.{train|construct|create}Percent ×global *_PRODUCTION_PERCENT`, ×`getBuildingCostModifier` (combiner), ×AI-handicap (perEra ramp + world/standard %), ÷100, ×AI-option discount; `max(1,·)`. Unit adds the `iInstanceCostModifier` count-ramp.
- **research cost** `CvTeam::getResearchCost` (~2581): `base×100 ×TECH_COST_MODIFIER ×gamespeed ×era.researchPercent ×(teamMember) + cuttingEdge + AI-handicap + upscaled`, ÷100. (Per-player `calculateResearchModifier` = diffusion/welfare, ≤+100% — a RATE modifier, not the cost.)
- **durations**: `getCivicAnarchyLength` (~8937) = `Σ civic.anarchyLength×100 − qtyDiscount ×gamespeed + worldSize ×anarchyMod ×civicAnarchyMod ×era.anarchyPercent`, ÷2 rebel, clamp `[minAnarchy, max]`. `getGoldenAgeLength` = `getModifiedIntValue(game.goldenAgeLength100, goldenAgeMod)/100`. religionAnarchy similar.
- **missionYieldMultiplier**: `adaptValueToGame(ADAPT_UNIT_YIELD)` = `value × gamespeed.getUnitYieldScalePercent / 100`. **hammerCostPercent** = `gamespeed.speedPercent ×(UPSCALED if option)`.
- GameSpeed has 3 distinct scalers by use: `hammerCostPercent` (production), `speedPercent` (research/anarchy), `unitYieldScalePercent` (mission yields).

### 10.3 BUILDING-LEVEL CITY families
- **cityCapture**: National (`player.getExtraNationalCaptureProbability/ResistanceModifier`) + Local (`city.getExtraLocalCaptureProbability/ResistanceModifier`), %; capturing a CITY (≠ unit `capture`).
- **pillageGold**: building `m_iPillageGoldModifier` is **ORPHANED — stored but NEVER aggregated** (no city/player path; the live one is unit-side `getPillageChange`). **Dead/unwired field → §8 dead-data candidate.**
- **occupationTime**: `occupationTimer = (BASE_OCCUPATION_TURNS + √pop) ×gamespeed ×cultureDamp`, then `getModifiedIntValue(·, occupationTimeModifier)` at capture (building source).
- **espionageDefense**: `city.getEspionageDefenseModifier` = `m_i + player.getNationalEspionageDefense` (building source).
- **populationGrowthRate**: ⚠ `getPopulationgrowthratepercentage` = `exp(m_fPopulationgrowthratepercentageLog)×100 − 100` — **FLOAT LOG-SPACE, multiplicative (OOS-relevant!)**; building source via log accumulation. Feeds the §9.3 growth threshold.
- **healing**: `getHealRate` (`m_iHealRate`, building `getHealRateChange`) + `getHealUnitCombatTypeTotal(UC)` (building `HealUnitCombatType` array).

### 10.4 CROSSOVERS / spawnRate / stateReligion
- **byOccupant**: **military happiness ONLY** (`getMilitaryHappiness` = `militaryHappinessUnits × player.getHappyPerMilitaryUnit`). **⚠ `celebrity happiness` DOES NOT EXIST in the code** — the spec/inventory "celebrity" member is vestigial; drop it.
- **byCargo**: **does NOT exist as an economic family** — cargo is transport (space/type) only. Drop from the inventory.
- **spawnRate**: **event-driven per-plot RNG** (`CvGame` ~6375) — civ `spawnRateModifier`/`npcPeaceModifier` + `CvSpawnInfo.turnRate` set a per-plot probability. **Not a per-turn value-channel** (a stochastic event, like a `grants.repeatable` chance).
- **stateReligion**: deterministic CONDITIONAL (gate `getStateReligion()!=NO_RELIGION ∧ isHasReligion`), **6 sub-modifiers routed through their host families**: unitProduction, buildingProduction, buildingCommerce, happiness, greatPeopleRate (holy-city), and **`HolyCityXPModifier` → feeds `getUnitCombatFreeExperience`** (a free-XP source beyond buildings/civics/traits). Not its own channel — a predicate gate on existing families.

---

**✅ THE MAP IS COMPLETE (waves 1-3, 2026-06-19).** Every per-turn value calc is mapped to its realized getter + per-source + gotchas; the dedup map (§7) + the family inventory (§8) round it out. **Emulator-relevant verdicts:** property + culture-spread + cultureDistance are SPATIAL (#429, out of containment); revolution is Python/non-deterministic (deferred); spawnRate is a stochastic event (not a value-channel); `celebrity happiness`/`byCargo` don't exist; `pillageGold` (building) is dead; `populationGrowthRate` is float (OOS care). The remaining channels are deterministic integer calcs the emulator can reproduce.

---

*Build order off this map: cityInput already does yields + commerce; add health/happiness → defense → maintenance (city channels, one dump); add `unitInput` (aggregate stats + promotion/unitcombat lists); then the in-flight + unmapped families as they land. Each channel gets a modcalc fidelity guard. One compile, one live verify across all (calc-emulator-spec §3/§5).*
