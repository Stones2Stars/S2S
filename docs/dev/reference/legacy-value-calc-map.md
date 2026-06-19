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

*Build order off this map: cityInput already does yields; add commerce → health/happiness → defense → maintenance (city channels, one dump); add `unitInput` (aggregate stats + promotion/unitcombat lists). Each channel gets a modcalc fidelity guard. One compile, one live verify across all (calc-emulator-spec §3/§5).*
